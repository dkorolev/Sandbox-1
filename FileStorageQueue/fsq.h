#ifndef FSQ_H
#define FSQ_H

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include "fsq_types.h"
#include "fsq_config.h"

#include "../Bricks/file/file.h"
#include "../Bricks/time/time.h"

namespace fsq {

// `FSQParamsFromFlags` is an initializer invoked from the default, empty, constructor of FSQ.
// It uses parameters from command-line flags, which is not best for some platforms.
// It is not always linked in. In order to use it, the user has to:
// 1) `#include "fsq_flags.h"`,
// 2) Do it only once per binary, to not violate the ODR.
// 3) initialize the flags library with the passed in { argc, argv }.
// An alternative option is to not use the default constructor of FSQ and pass in the params manually,
// which is the recommended route when using FSQ on a mobile device.
template <class CONFIG>
struct FSQParamsFromFlags;

// Class FSQ manages local, filesystem-based message queue.
// A temporary append-only file is created and then written into. Once the policy dictates so,
// it is declared finalized and gets atomically renamed to a different name (with its 1st timestamp in it),
// using which name it is passed to the PROCESSOR. A new new append-only file is started in the meantime.
// The processor runs in a dedicated thread. Thus, it is guaranteed to process at most one file at a time.
// It can take as long as it needs to process the file. Files are guaranteed to be passed in the FIFO order.
// If PROCESSOR returns true, file processing is declared successful and FQS erases it.
// If PROCESSOR returns false, file is kept and will be re-sent to the processor, with respect to the
// retry policy specified as the template parameter to FSQ.
// On top of the above FSQ supports purging old files accorging to the policy.
template <class CONFIG>
class FSQ final {
 public:
  typedef CONFIG T_CONFIG;
  typedef typename T_CONFIG::T_PROCESSOR T_PROCESSOR;
  template <class TIME_MANAGER, class FILE_SYSTEM>
  using T_RETRY_POLICY = typename T_CONFIG::template T_RETRY_POLICY<TIME_MANAGER, FILE_SYSTEM>;
  typedef typename T_CONFIG::T_FINALIZE_POLICY T_FINALIZE_POLICY;
  typedef typename T_CONFIG::T_PURGE_POLICY T_PURGE_POLICY;
  typedef typename T_CONFIG::T_MESSAGE T_MESSAGE;
  typedef typename T_CONFIG::T_FILE_APPEND_POLICY T_FILE_APPEND_POLICY;
  typedef typename T_CONFIG::T_TIME_MANAGER T_TIME_MANAGER;
  typedef typename T_TIME_MANAGER::T_TIMESTAMP T_TIMESTAMP;
  typedef typename T_CONFIG::T_FILE_SYSTEM T_FILE_SYSTEM;

  typedef FSQParams<T_CONFIG> params_type;

  FSQ(T_PROCESSOR& processor,
      T_TIME_MANAGER& time_manager,
      T_FILE_SYSTEM& file_system,
      params_type params = FSQParamsFromFlags<T_CONFIG>::Construct())
      : params_(params),
        processor_(processor),
        time_manager_(time_manager),
        file_system_(file_system),
        processor_thread_(&FSQ::ExporterThread, this) {
    // TODO(dkorolev): Get current file name, timestamp and length.
  }

  ~FSQ() {
    // Notify the thread that FSQ is being terminated.
    {
      std::unique_lock<std::mutex> lock(mutex_);
      destructing_ = true;
    }
    condition_variable_.notify_all();
    // Either wait for the processor thread to terminate or detach it.
    if (T_CONFIG::DetachProcessingThreadOnTermination()) {
      processor_thread_.detach();
    } else {
      processor_thread_.join();
    }
  }

  void OnMessage(const T_MESSAGE& message, size_t /*dropped_messages*/ = 0) {
    const T_TIMESTAMP timestamp = time_manager_.MockableNow();
    ValidateCurrentFile(message.size(), timestamp);
    // TODO(dkorolev): Update stats accordingly.
    // file_system_.AppendToFile(current_filename_, message);
    // current_file_length_ += message.length();
  }

 private:
  // ValidateCurrentFiles() expires the current file and/or creates the new one as necessary.
  void ValidateCurrentFile(const size_t new_message_length, const T_TIMESTAMP timestamp) {
    /*
    if (!current_filename_.empty() && (current_file_length_ + new_message_length >= params_.max_file_size ||
                                       current_file_first_timestamp_ + params_.max_file_age <= timestamp)) {
    */
    if (false) {
      // TODO(dkorolev): Include timestamps in file name.
      // const std::string committed_filename = params_.committed_filename;

      // file_system_.RenameFile(current_filename_, committed_filename);

      // ValidateCurrentFile is called from OnMessage() only.
      // Thus, as long as OnMessage() is not reentrant (which is the case for the message queue we use),
      // ValidateCurrentFile() is also not reentrant, and thus does not require locking.
      if (processor_.ReadyToAcceptData()) {
        condition_variable_.notify_all();
      }

      // current_filename_.clear();
    }

    /*
    if (current_filename_.empty()) {
      current_filename_ = params_.current_filename;  // TODO(dkorolev): Timestamp it.
      file_system_.CreateFile(current_filename_);
      current_file_length_ = 0;
      current_file_first_timestamp_ = current_file_last_timestamp_ = timestamp;
    }
    */
  }

  void ExporterThread() {
    // TODO(dkorolev): Initial directory scan.
    std::unique_lock<std::mutex> lock(mutex_);
    while (true) {
      // TODO(dkorolev): Code and test file scan and processing.
      if (destructing_) {
        return;
      }
      // TODO(dkorolev): And scan files.
      condition_variable_.wait(lock, [this] { return destructing_ && !processor_.ReadyToAcceptData(); });
      if (destructing_) {
        return;
      }
      // TODO(dkorolev): If ready to accept, process data from the new file.
    }
  }

  const params_type params_;
  T_PROCESSOR& processor_;
  T_TIME_MANAGER& time_manager_;
  T_FILE_SYSTEM& file_system_;

  /*
  std::string current_filename_;
  uint64_t current_file_length_ = 0;
  uint64_t current_file_first_timestamp_ = 0;
  uint64_t current_file_last_timestamp_ = 0;
  */

  std::thread processor_thread_;
  std::mutex mutex_;
  std::condition_variable condition_variable_;
  bool destructing_ = false;

  FSQ(const FSQ&) = delete;
  FSQ(FSQ&&) = delete;
  void operator=(const FSQ&) = delete;
  void operator=(FSQ&&) = delete;
};

}  // namespace fsq

#endif  // FSQ_H
