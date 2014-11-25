#ifndef SANDBOX_CLIENT_FILE_STORAGE_H
#define SANDBOX_CLIENT_FILE_STORAGE_H

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

struct PosixFileManager final {
  // TODO(dkorolev): Add wrappers over posix methods we need here.
};

struct CPPChrono final {
  typedef uint64_t T_TIMESTAMP;
  T_TIMESTAMP wall_time() const {
    return static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::system_clock::now().time_since_epoch()).count());
  }
};

// Default initializer. Requires users to `#include "client_file_storage_flags.h"`.
// Otherwise, compile error would occur if no explicit `Params` are passed to the constructor.
template <typename EXPORTER, typename MESSAGE, typename TIME_MANAGER, typename FILE_MANAGER>
struct ClientFileStorageParamsFromFlags;

template <typename EXPORTER,
          typename MESSAGE = std::string,
          typename TIME_MANAGER = CPPChrono,
          typename FILE_MANAGER = PosixFileManager>
class ClientFileStorage final {
 public:
  typedef EXPORTER T_EXPORTER;
  typedef MESSAGE T_MESSAGE;
  typedef TIME_MANAGER T_TIME_MANAGER;
  typedef typename T_TIME_MANAGER::T_TIMESTAMP T_TIMESTAMP;
  typedef FILE_MANAGER T_FILE_MANAGER;

  struct Params {
#ifdef PARAM
#error "'PARAM' should not be defined by this point."
#else
#define PARAM(type, param)          \
  type param;                       \
  Params& set_##param(type value) { \
    param = value;                  \
    return *this;                   \
  }
    PARAM(std::string, current_filename);
    PARAM(std::string, committed_filename);
    PARAM(typename T_TIME_MANAGER::T_TIMESTAMP, max_file_age);
    PARAM(uint64_t, max_file_size);
#undef PARAM
#endif
  };

  static Params FromFlags() {
    return ClientFileStorageParamsFromFlags<T_EXPORTER, T_MESSAGE, T_TIME_MANAGER, T_FILE_MANAGER>::Construct();
  }

  ClientFileStorage(T_EXPORTER& exporter,
                    T_TIME_MANAGER& time_manager,
                    T_FILE_MANAGER& file_manager,
                    Params params = FromFlags())
      : params_(params),
        exporter_(exporter),
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
    const T_TIMESTAMP timestamp = time_manager_.wall_time();
    ValidateCurrentFile(message.size(), timestamp);
    file_manager_.AppendToFile(current_filename_, message);
    current_file_length_ += message.length();
    // TODO(dkorolev): Handle dropped_messages.
  }

 private:
  // ValidateCurrentFiles() expires the current file and/or creates a new one as necessary.
  void ValidateCurrentFile(const size_t new_message_length, const T_TIMESTAMP timestamp) {
    if (!current_filename_.empty() && (current_file_length_ + new_message_length >= params_.max_file_size ||
                                       current_file_first_ + params_.max_file_age <= timestamp)) {
      // TODO(dkorolev): Include timestamps in file name.
      const std::string committed_filename = params_.committed_filename;

      file_manager_.RenameFile(current_filename_, committed_filename);

      // ValidateCurrentFile is called from OnMessage() only.
      // Thus, as long as OnMessage() is not reentrant (which is the case for the message queue we use),
      // ValidateCurrentFile() is also not reentrant, and thus does not require locking.
      if (exporter_.ReadyToAcceptData()) {
        condition_variable_.notify_all();
      }

      current_filename_.clear();
    }

    if (current_filename_.empty()) {
      current_filename_ = params_.current_filename;  // TODO(dkorolev): Timestamp it.
      file_manager_.CreateFile(current_filename_);
      current_file_length_ = 0;
      current_file_first_ = current_file_last_ = timestamp;
    }
  }

  void ExporterThread() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (true) {
      // TODO(dkorolev): Code and test file scan and processing.
      if (destructing_) {
        return;
      }
      // TODO(dkorolev): And scan files.
      condition_variable_.wait(lock, [this] { return destructing_ && !exporter_.ReadyToAcceptData(); });
      if (destructing_) {
        return;
      }
      // TODO(dkorolev): If ready to accept, process data from the new file.
    }
  }

  Params params_;
  T_EXPORTER& exporter_;
  T_TIME_MANAGER& time_manager_;
  T_FILE_MANAGER& file_manager_;

  // TODO(dkorolev): Add it.
  std::string current_filename_;
  uint64_t current_file_length_ = 0;
  uint64_t current_file_first_ = 0;
  uint64_t current_file_last_ = 0;

  std::thread exporter_thread_;
  std::mutex mutex_;
  std::condition_variable condition_variable_;
  bool destructing_ = false;

  ClientFileStorage(const ClientFileStorage&) = delete;
  ClientFileStorage(ClientFileStorage&&) = delete;
  void operator=(const ClientFileStorage&) = delete;
  void operator=(ClientFileStorage&&) = delete;
};

#endif  // SANDBOX_CLIENT_FILE_STORAGE_H
