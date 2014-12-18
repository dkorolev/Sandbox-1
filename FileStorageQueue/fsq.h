// Class FSQ manages local, filesystem-based message queue.
//
// A temporary append-only file is created and then written into. Once the strategy dictates so,
// it is declared finalized and gets atomically renamed to a different name (with its 1st timestamp in it),
// using which name it is passed to the PROCESSOR. A new new append-only file is started in the meantime.
//
// The processor runs in a dedicated thread. Thus, it is guaranteed to process at most one file at a time.
// It can take as long as it needs to process the file. Files are guaranteed to be passed in the FIFO order.
//
// Once a file is ready, which translates to "on startup" if there are pending files,
// the user handler in PROCESSOR::OnFileReady(file_name) is invoked.
// Further logic depends on its return value:
//
// On `Success`, FQS deleted file that just got processed and sends the next one to as it arrives,
// which can be instantaneously, is the queue is not empty, or once the next file is ready, if it is.
//
// On `SuccessAndMoved`, FQS does the same thing as for `Success`, except for it does not attempt
// to delete the file, assuming that it has already been deleted or moved away by the user code.
//
// On `Unavailable`, automatic file processing is suspended until it is resumed externally.
// An example of this case would be the processor being the file uploader, with the device going offline.
// This way, no further action is required until FQS is explicitly notified that the device is back online.
//
// On `FailureNeedRetry`, the file is kept and will be re-attempted to be sent to the processor,
// with respect to the retry strategy specified as the template parameter to FSQ.
//
// On top of the above FSQ keeps an eye on the size it occupies on disk and purges the oldest data files
// if the specified purge strategy dictates so.

#ifndef FSQ_H
#define FSQ_H

#include <cassert>  // TODO(dkorolev): Perhaps introduce exceptions instead of ASSERT-s?

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include "status.h"
#include "config.h"
#include "strategies.h"

#include "../Bricks/file/file.h"
#include "../Bricks/time/time.h"

namespace fsq {

enum class FileProcessingResult { Success, SuccessAndMoved, Unavailable, FailureNeerRetry };

template <class CONFIG>
class FSQ final : public CONFIG::T_FILE_NAMING_STRATEGY,
                  public CONFIG::T_FINALIZE_POLICY,
                  public CONFIG::T_PURGE_POLICY,
                  public CONFIG::T_FILE_APPEND_POLICY {
 public:
  typedef CONFIG T_CONFIG;
  typedef typename T_CONFIG::T_PROCESSOR T_PROCESSOR;
  typedef typename T_CONFIG::T_FILE_NAMING_STRATEGY T_FILE_NAMING_STRATEGY;
  template <class TIME_MANAGER, class FILE_SYSTEM>
  using T_RETRY_POLICY = typename T_CONFIG::template T_RETRY_POLICY<TIME_MANAGER, FILE_SYSTEM>;
  typedef typename T_CONFIG::T_FINALIZE_POLICY T_FINALIZE_POLICY;
  typedef typename T_CONFIG::T_PURGE_POLICY T_PURGE_POLICY;
  typedef typename T_CONFIG::T_MESSAGE T_MESSAGE;
  typedef typename T_CONFIG::T_FILE_APPEND_POLICY T_FILE_APPEND_POLICY;
  typedef typename T_CONFIG::T_TIME_MANAGER T_TIME_MANAGER;
  typedef typename T_TIME_MANAGER::T_TIMESTAMP T_TIMESTAMP;
  typedef typename T_TIME_MANAGER::T_TIME_SPAN T_TIME_SPAN;
  typedef typename T_CONFIG::T_FILE_SYSTEM T_FILE_SYSTEM;

  typedef QueueFinalizedFilesStatus<T_TIMESTAMP, T_TIME_SPAN> FinalizedFilesStatus;
  typedef QueueStatus<T_TIMESTAMP, T_TIME_SPAN> Status;

  FSQ(T_PROCESSOR& processor,
      const std::string& working_directory,
      const T_TIME_MANAGER& time_manager = T_TIME_MANAGER(),
      const T_FILE_SYSTEM& file_system = T_FILE_SYSTEM())
      : processor_(processor),
        time_manager_(time_manager),
        file_system_(file_system),
        working_directory_(working_directory),
        processor_thread_(&FSQ::ProcessorThread, this) {
    // TODO(dkorolev): Get current file name, timestamp and length.
  }

  ~FSQ() {
    // Notify the thread that FSQ is being terminated.
    {
      std::unique_lock<std::mutex> lock(mutex_);
      destructing_ = true;
      condition_variable_.notify_all();
    }
    // Close the current file.
    current_file_.reset(nullptr);
    // Either wait for the processor thread to terminate or detach it.
    if (T_CONFIG::DetachProcessingThreadOnTermination()) {
      processor_thread_.detach();
    } else {
      processor_thread_.join();
    }
  }

  void PushMessage(const T_MESSAGE& message) {
    if (destructing_) {
      // TODO(dkorolev): Chat with Alex. Exception?
      return;
    } else {
      const T_TIMESTAMP now = time_manager_.Now();
      const uint64_t message_size_in_bytes = T_FILE_APPEND_POLICY::MessageSizeInBytes(message);
      EnsureCurrentFileIsOpen(message_size_in_bytes, now);
      assert(current_file_.get());
      assert(!current_file_->bad());
      T_FILE_APPEND_POLICY::AppendToFile(*current_file_.get(), message);
      status_.appended_file_size += message_size_in_bytes;
      status_.appended_file_age = now - current_file_creation_time_;
    }
  }

  void ForceResumeProcessing() {
    // TODO(dkorolev): Don't have to rename the current file unless it's the only one.
    if (current_file_.get()) {
      current_file_.reset(nullptr);
      status_.appended_file_size = 0;
      status_.appended_file_age = T_TIME_SPAN(0);
      const std::string finalized_file_name =
          working_directory_ + '/' +
          T_FILE_NAMING_STRATEGY::GenerateFinalizedFileName(current_file_creation_time_);
      T_FILE_SYSTEM::RenameFile(current_file_name_, finalized_file_name);
      current_file_name_.clear();
    }
    {
      std::unique_lock<std::mutex> lock(mutex_);
      has_new_file_ = true;
      condition_variable_.notify_all();
    }
  }

  const std::string& WorkingDirectory() const {
    return working_directory_;
  }

  const Status& GetQueueStatus() const {
    // TODO(dkorolev): Wait until the 1st scan, running in a different thread, has finished.
    return status_;
  }

  FinalizedFilesStatus RescanDir() const {
    FinalizedFilesStatus finalized_files_status;
    T_FILE_SYSTEM::ScanDir(working_directory_, [this, &finalized_files_status](const std::string& file_name) {
      if (T_FILE_NAMING_STRATEGY::template IsFinalizedFileName<T_TIMESTAMP>(file_name)) {
        const uint64_t file_size = T_FILE_SYSTEM::GetFileSize(working_directory_ + '/' + file_name);
        ++finalized_files_status.number_of_queued_files;
        finalized_files_status.total_queued_files_size += file_size;
        if (finalized_files_status.oldest_queued_file_name.empty()) {
          finalized_files_status.oldest_queued_file_name = file_name;
          finalized_files_status.oldest_queued_file_size = file_size;
          // TODO(dkorolev): Fill in more fields.
        }
        // TODO(dkorolev): Pick the oldest file.
        // TODO(dkorolev): Update `oldest_queued_file_timestamp`.
      }
    });
    return finalized_files_status;
  }

 private:
  // ValidateCurrentFiles() expires the current file and/or creates the new one as necessary.
  void EnsureCurrentFileIsOpen(const uint64_t /*message_size_in_bytes*/, const T_TIMESTAMP now) {
    // TODO(dkorolev): Purge.
    if (!current_file_.get()) {
      current_file_name_ = working_directory_ + '/' + T_FILE_NAMING_STRATEGY::GenerateCurrentFileName(now);
      current_file_.reset(new typename T_FILE_SYSTEM::OutputFile(current_file_name_));
      current_file_creation_time_ = now;
    }
  }

  void ProcessorThread() {
    // TODO(dkorolev): Initial directory scan.
    while (true) {
      // TODO(dkorolev): Code and test file scan and processing.
      {
        std::unique_lock<std::mutex> lock(mutex_);
        if (destructing_) {
          return;
        }
      }

      {
        std::unique_lock<std::mutex> lock(mutex_);

        // TODO(dkorolev): Add retransmission delay here.
        if (!has_new_file_) {
          condition_variable_.wait(lock);
        }
      }

      {
        if (destructing_) {
          return;
        }

        has_new_file_ = false;
        const FinalizedFilesStatus finalized_files_status = RescanDir();
        status_.UpdateFinalizedFileStatus(finalized_files_status);

        if (!finalized_files_status.oldest_queued_file_name.empty()) {
          const FileProcessingResult result = processor_.template OnFileReady<T_TIMESTAMP, T_TIME_SPAN>(
              working_directory_ + '/' + finalized_files_status.oldest_queued_file_name,
              finalized_files_status.oldest_queued_file_name,
              finalized_files_status.oldest_queued_file_size,
              finalized_files_status.oldest_queued_file_timestamp,  // TODO(dkorolev): Fill these fields.
              finalized_files_status.oldest_queued_file_age,
              time_manager_.Now());
          static_cast<void>(result);
        }
      }
      // TODO(dkorolev): If ready to accept, process data from the new file.
    }
  }

  //  const Params params_;
  Status status_;

  T_PROCESSOR& processor_;
  const T_TIME_MANAGER& time_manager_;
  const T_FILE_SYSTEM& file_system_;
  std::string working_directory_;

  std::unique_ptr<typename T_FILE_SYSTEM::OutputFile> current_file_;
  std::string current_file_name_;
  T_TIMESTAMP current_file_creation_time_;

  std::thread processor_thread_;
  std::mutex mutex_;
  std::condition_variable condition_variable_;
  bool has_new_file_ = false;
  bool destructing_ = false;

  FSQ(const FSQ&) = delete;
  FSQ(FSQ&&) = delete;
  void operator=(const FSQ&) = delete;
  void operator=(FSQ&&) = delete;
};

}  // namespace fsq

#endif  // FSQ_H
