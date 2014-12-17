// TODO(dkorolev): Rename this file.

#ifndef SANDBOX_CLIENT_FILE_STORAGE_POLICIES_H
#define SANDBOX_CLIENT_FILE_STORAGE_POLICIES_H

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include "client_file_storage.h"  // Need the definition of QueueStatus.

// Default retry policy.
// Retry after an amount of time drawn from an exponential distribution
// with the mean defaulting to 15 minutes, min defaulting to 1 minute and max defaulting to 24 hours.
// Handles time skews correctly.
template <typename TIME_MANAGER_FOR_RETRY_POLICY, typename FILE_SYSTEM_FOR_RETRY_POLICY>
class RetryExponentially {
 public:
  typedef TIME_MANAGER_FOR_RETRY_POLICY T_TIME_MANAGER;
  typedef FILE_SYSTEM_FOR_RETRY_POLICY T_FILE_SYSTEM;
  struct Params {
    double mean, min, max;
    Params(double mean, double min, double max) : mean(mean), min(min), max(max) {
    }
    Params() = default;
    Params(const Params&) = default;
    const Params& operator=(const Params&) = default;
  };
  explicit RetryExponentially(const T_TIME_MANAGER& time_manager,
                              const T_FILE_SYSTEM& file_system,
                              const Params& params)
      : time_manager_(time_manager),
        last_update_time_(time_manager_.MockableNow()),
        time_to_be_ready_to_process_(last_update_time_),
        file_system_(file_system),
        params_(params) {
    // TODO(dkorolev): Code it.
    // SetUpDistribution();
  }
  explicit RetryExponentially(const T_TIME_MANAGER& time_manager,
                              const T_FILE_SYSTEM& file_system,
                              const double mean = 15 * 60 * 1e3,
                              const double min = 60 * 1e3,
                              const double max = 24 * 60 * 60 * 1e3)
      : RetryExponentially(time_manager, file_system, Params(mean, min, max)) {
  }
  void AttachToFile(const std::string filename) {
    // Serializes and deserializes itself into a file, used to preserve retry delays between restarts.
    // TODO(dkorolev): Support other means like CoreData, or stick with a file?
  }
  bool ReadyToProcess() const {
    const typename T_TIME_MANAGER::T_TIMESTAMP now = time_manager_.MockableNow();
    if (now < last_update_time_) {
      // Possible time skew, stay on the safe side.
      last_update_time_ = now;
      time_to_be_ready_to_process_ = now;
      return true;
    } else {
      return now >= time_to_be_ready_to_process_;
    }
  }
  // OnSuccess(): Clear all retry delays, cruising at full speed.
  void OnSuccess() {
    last_update_time_ = time_manager_.MockableNow();
    time_to_be_ready_to_process_ = last_update_time_;
  }
  // OnFailure(): Set or update all retry delays.
  void OnFailure() {
    const typename T_TIME_MANAGER::T_TIMESTAMP now = time_manager_.MockableNow();
    if (now < last_update_time_) {
      // Possible time skew, stay on the safe side.
      time_to_be_ready_to_process_ = now;
    }
    last_update_time_ = now;
    // TODO(dkorolev): Code it.
    time_to_be_ready_to_process_ =
        std::max(time_to_be_ready_to_process_, now + 1000);  // now + DrawFromExponentialDistribution();
  }

 private:
  const T_TIME_MANAGER& time_manager_;
  const T_FILE_SYSTEM& file_system_;
  mutable typename T_TIME_MANAGER::T_TIMESTAMP last_update_time_;
  mutable typename T_TIME_MANAGER::T_TIMESTAMP time_to_be_ready_to_process_;
  const Params params_;
};

// Default file finalization policy.
struct KeepFilesAround100KBUnlessNoBacklog {
  typedef bricks::time::MILLISECONDS_INTERVAL DELTA_MS;
  // The default implementation only supports MILLISECOND as timestamps.
  bool ShouldFinalize(const QueueStatus<DELTA_MS>& status) const {
    if (status.appended_file_size >= 100 * 1024 * 1024 ||
        status.appended_file_age > DELTA_MS(24 * 60 * 60 * 1000)) {
      // Always keep files of at most 100KB and at most 24 hours old.
      return true;
    } else if (status.number_of_queued_files > 0) {
      // The above is the only condition as long as there are queued, pending, unprocessed files.
      return false;
    } else {
      // Otherwise, there are no files pending processing no queue,
      // and the default policy can be legitimately expected to keep finalizing files somewhat often.
      return (status.appended_file_size >= 10 * 1024 * 1024 ||
              status.appended_file_age > DELTA_MS(10 * 60 * 1000));
    }
  }
};

// Default file purge policy.
struct KeepUnder1GBAndUnder1KFiles {
  bool ShouldPurge(const QueueStatus<bricks::time::UNIX_TIME_MILLISECONDS>& status) const {
    if (status.total_queued_files_size + status.appended_file_size > 1024 * 1024 * 1024) {
      // Purge the oldest queued files if the total size of data stored in the queue exceeds 1GB.
      return true;
    } else if (status.number_of_queued_files > 1000) {
      // Purge the oldest queued files if the total number of queued files exceeds 1000.
      return true;
    } else {
      // Good to go otherwise.
      return false;
    }
  }
};

// Default file append policy.
// Just append without any separators.
struct JustAppend {
  // Appends data to file with no strings attached.
};

// Default time policy.
// Use UNIX time as milliseconds.
struct CPPChrono final {
  typedef bricks::time::UNIX_TIME_MILLISECONDS T_TIMESTAMP;
  T_TIMESTAMP MockableNow() const {
    return bricks::time::Now();
  }
};

#endif  // SANDBOX_CLIENT_FILE_STORAGE_POLICIES_H
