// TODO(dkorolev): Rename this file and classes.
// TODO(dkorolev): Ensure that the directory is scanned from within a dedicated thread.

#ifndef SANDBOX_CLIENT_FILE_STORAGE_H
#define SANDBOX_CLIENT_FILE_STORAGE_H

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include "client_file_storage_types.h"
#include "client_file_storage_config.h"

#include "../Bricks/file/file.h"
#include "../Bricks/time/time.h"

// Default initializer invoked from the default constructor of ClientFileStorage.
// Uses parameters from command-line flags. Requires requires users to
// 1) `#include "client_file_storage_flags.h"`, and
// 2) initialize the flags library with the passed in { argc, argv }.
// Another option is to not use the default constructor of ClientFileStorage and pass in the params manually.
// This split is to eliminate the dependency on command line flags, which are not used on mobile devices.
template <class CONFIG>
struct ClientFileStorageParamsFromFlags;

// Class ClientFileStorage manages local, filesystem-based message queue.
// A temporary file is created and then only appended to. Once the policy dictates so,
// it is declared finalized and thus atomically renamed under a different name,
// using which it is passed to the processor, with the new append-only flie being started.
// The processor runs in a dedicated thread, and may safely take its time to process the file.
// If the processor returns so, the file is removed by ClientFileStorage. If the processor
// returns false, the file is kept and then retried at some point later according to the retry policy.
template <class CONFIG>
class ClientFileStorage final {
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

  typedef ClientFileStorageParams<T_CONFIG> params_type;

  ClientFileStorage(T_PROCESSOR& exporter,
                    T_TIME_MANAGER& time_manager,
                    T_FILE_SYSTEM& file_system,
                    params_type params = ClientFileStorageParamsFromFlags<T_CONFIG>::Construct())
      : params_(params),
        exporter_(exporter),
        time_manager_(time_manager),
        file_system_(file_system),
        exporter_thread_(&ClientFileStorage::ExporterThread, this) {
    // TODO(dkorolev): Get current file name, timestamp and length.
  }
  ~ClientFileStorage() {
    // Wait until the exporter thread terminates gracefully.
    {
      std::unique_lock<std::mutex> lock(mutex_);
      destructing_ = true;
    }
    condition_variable_.notify_all();
    exporter_thread_.join();
  }

  void OnMessage(const T_MESSAGE& message, size_t /*dropped_messages*/) {
    const T_TIMESTAMP timestamp = time_manager_.MockableNow();
    ValidateCurrentFile(message.size(), timestamp);
    file_system_.AppendToFile(current_filename_, message);
    current_file_length_ += message.length();
    // TODO(dkorolev): Handle dropped_messages.
  }

 private:
  // ValidateCurrentFiles() expires the current file and/or creates the new one as necessary.
  void ValidateCurrentFile(const size_t new_message_length, const T_TIMESTAMP timestamp) {
    if (!current_filename_.empty() && (current_file_length_ + new_message_length >= params_.max_file_size ||
                                       current_file_first_timestamp_ + params_.max_file_age <= timestamp)) {
      // TODO(dkorolev): Include timestamps in file name.
      const std::string committed_filename = params_.committed_filename;

      file_system_.RenameFile(current_filename_, committed_filename);

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
      file_system_.CreateFile(current_filename_);
      current_file_length_ = 0;
      current_file_first_timestamp_ = current_file_last_timestamp_ = timestamp;
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

  params_type params_;
  T_PROCESSOR& exporter_;
  T_TIME_MANAGER& time_manager_;
  T_FILE_SYSTEM& file_system_;

  // TODO(dkorolev): Add it.
  std::string current_filename_;
  uint64_t current_file_length_ = 0;
  uint64_t current_file_first_timestamp_ = 0;
  uint64_t current_file_last_timestamp_ = 0;

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
