#ifndef SANDBOX_CLIENT_FILE_STORAGE_H
#define SANDBOX_CLIENT_FILE_STORAGE_H

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

// TODO(dkorolev): Migrate to header-only gflags.
#include <gflags/gflags.h>

DEFINE_string(current_filename,
              "current",
              "The name of the file to be appended to.");  // TODO(dkorolev): Timestamp it.

DEFINE_string(committed_filename,
              "done",
              "The name of the file to rename completed files into.");  // TODO(dkorolev): Timestamp them.

DEFINE_int64(max_file_age_ms,
             1000 * 60 * 60 * 4,
             "Start a new file as the first entry of the current one is this number of milliseconds old. "
             "Defaults to 4 hours.");

DEFINE_int64(max_file_size,
             1024 * 1024 * 256,
             "Start a new file after this size of the current one exceeds this. Defaults to 256KB.");

struct PosixFileManager final {
  // TODO(dkorolev): Add wrappers over posix methods we need here.
};

struct CPPChrono final {
  typedef uint64_t T_MS;
  T_MS wall_time_ms() const {
    return static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::system_clock::now().time_since_epoch()).count());
  }
};

template <typename EXPORTER,
          typename MESSAGE = std::string,
          typename TIME_MANAGER = CPPChrono,
          typename FILE_MANAGER = PosixFileManager>
class ClientFileStorage final {
 public:
  typedef EXPORTER T_EXPORTER;
  typedef MESSAGE T_MESSAGE;
  typedef TIME_MANAGER T_TIME_MANAGER;
  typedef typename T_TIME_MANAGER::T_MS T_MS;
  typedef FILE_MANAGER T_FILE_MANAGER;
  ClientFileStorage(T_EXPORTER& exporter, T_TIME_MANAGER& time_manager, T_FILE_MANAGER& file_manager)
      : exporter_(exporter),
        time_manager_(time_manager),
        file_manager_(file_manager),
        exporter_thread_(&ClientFileStorage::ExporterThread, this) {
    // TODO(dkorolev): Get current file name, timestamp and length.
  }
  ~ClientFileStorage() {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      destructing_ = true;
    }
    condition_variable_.notify_all();
    exporter_thread_.join();
  }

  void OnMessage(const T_MESSAGE& message, size_t dropped_messages) {
    const T_MS timestamp_ms = time_manager_.wall_time_ms();
    ValidateCurrentFile(message.size(), timestamp_ms);
    file_manager_.AppendToFile(current_filename_, message);
    current_file_length_ += message.length();
    // TODO(dkorolev): Handle dropped_messages.
  }

 private:
  // ValidateCurrentFiles() expires the current file and/or creates a new one as necessary.
  void ValidateCurrentFile(const size_t new_message_length, const T_MS timestamp_ms) {
    if (!current_filename_.empty() && (current_file_length_ + new_message_length >= FLAGS_max_file_size ||
                                       current_file_first_ms_ + FLAGS_max_file_age_ms <= timestamp_ms)) {
      // TODO(dkorolev): Include timestamps in file name.
      const std::string committed_filename = FLAGS_committed_filename;

      file_manager_.RenameFile(current_filename_, committed_filename);

      {
        std::unique_lock<std::mutex> lock(mutex_);
        // TODO(dkorolev): Discuss this event with Alex. Keep a hack so far.
        next_file_.filename = committed_filename;
        // TODO(dkorolev): Add current_file_length_, current_file_first_ms_, current_file_last_ms_.
      }
      condition_variable_.notify_all();

      current_filename_.clear();
    }

    if (current_filename_.empty()) {
      current_filename_ = FLAGS_current_filename;  // TODO(dkorolev): Timestamp it.
      file_manager_.CreateFile(current_filename_);
      current_file_length_ = 0;
      current_file_first_ms_ = current_file_last_ms_ = timestamp_ms;
    }
  }

  void ExporterThread() {
    // TODO(dkorolev): Chat with Alex on how to best handle multiple files. What if WiFi is now off?
    std::unique_lock<std::mutex> lock(mutex_);
    while (true) {
      if (next_file_.filename.empty()) {
        if (destructing_) {
          return;
        }
        condition_variable_.wait(lock, [this] { return !next_file_.filename.empty() || destructing_; });
        if (destructing_) {
          return;
        }
      }
      // TODO(dkorolev): Handle the file.
      // exporter_.OnFileCommitted(
      //    committed_filename, current_file_length_, current_file_first_ms_, current_file_last_ms_);
      next_file_.filename.clear();
      // TODO(dkorolev): This file should be removed.
    }
  }

  T_EXPORTER& exporter_;
  T_TIME_MANAGER& time_manager_;
  T_FILE_MANAGER& file_manager_;

  // TODO(dkorolev): Add it.
  std::string current_filename_;
  uint64_t current_file_length_ = 0;
  uint64_t current_file_first_ms_ = 0;
  uint64_t current_file_last_ms_ = 0;

  std::thread exporter_thread_;
  bool destructing_ = false;
  // TODO(dkorolev): Rethink this with Alex: do pushing per-file or pass in a list of them?
  struct NextFileInfo {
    std::string filename;
  };
  NextFileInfo next_file_;
  std::mutex mutex_;
  std::condition_variable condition_variable_;

  ClientFileStorage(const ClientFileStorage&) = delete;
  ClientFileStorage(ClientFileStorage&&) = delete;
  void operator=(const ClientFileStorage&) = delete;
  void operator=(ClientFileStorage&&) = delete;
};

#endif  // SANDBOX_CLIENT_FILE_STORAGE_H
