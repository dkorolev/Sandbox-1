#ifndef FSQ_STRATEGIES_H
#define FSQ_STRATEGIES_H

#include <string>

#include "../Bricks/util/util.h"
#include "../Bricks/file/file.h"
#include "../Bricks/time/time.h"
#include "../Bricks/strings/fixed_size_serializer.h"

namespace fsq {
namespace strategy {

// Default file append strategy: Appends data to files in raw format, without separators.
struct JustAppendToFile {
  void AppendToFile(bricks::FileSystem::OutputFile& fo, const std::string& message) const {
    // TODO(dkorolev): Should we flush each record? Make it part of the strategy?
    fo << message << std::flush;
  }
  uint64_t MessageSizeInBytes(const std::string& message) const {
    return message.length();
  }
};

// Default file naming strategy: Use "finalized-{timestamp}.bin" and "current-{timestamp}.bin".
struct DummyFileNamingToUnblockAlexFromMinsk {
  struct FileNamingSchema {
    FileNamingSchema(const std::string& prefix, const std::string& suffix) : prefix_(prefix), suffix_(suffix) {
    }
    template <typename T_TIMESTAMP>
    inline std::string GenerateFileName(const T_TIMESTAMP timestamp) const {
      return prefix_ + bricks::strings::PackToString(timestamp) + suffix_;
    }
    template <typename T_TIMESTAMP>
    inline bool ParseFileName(const std::string& filename, T_TIMESTAMP* output_timestamp) const {
      if ((filename.length() ==
           prefix_.length() + bricks::strings::FixedSizeSerializer<T_TIMESTAMP>::size_in_bytes +
               suffix_.length()) &&
          filename.substr(0, prefix_.length()) == prefix_ &&
          filename.substr(filename.length() - suffix_.length()) == suffix_) {
        T_TIMESTAMP timestamp;
        bricks::strings::UnpackFromString(filename.substr(prefix_.length()), timestamp);
        if (GenerateFileName(timestamp) == filename) {
          *output_timestamp = timestamp;
          return true;
        } else {
          return false;
        }
      } else {
        return false;
      }
    }
    std::string prefix_;
    std::string suffix_;
  };
  FileNamingSchema current = FileNamingSchema("current-", ".bin");
  FileNamingSchema finalized = FileNamingSchema("finalized-", ".bin");
};

// Default time manager strategy: Use UNIX time in milliseconds.
struct UseUNIXTimeInMilliseconds final {
  typedef bricks::time::EPOCH_MILLISECONDS T_TIMESTAMP;
  typedef bricks::time::MILLISECONDS_INTERVAL T_TIME_SPAN;
  T_TIMESTAMP Now() const {
    return bricks::time::Now();
  }
};

// Default file finalization strategy: Keeps files under 100KB, if there is backlog,
// in case of no backlog keep them under 10KB. Also manage maximum age before forced finalization:
// a maximum of 24 hours when there is backlog, a maximum of 10 minutes if there is no.
template <typename TIMESTAMP,
          typename TIME_SPAN,
          uint64_t BACKLOG_MAX_FILE_SIZE,
          TIME_SPAN BACKLOG_MAX_FILE_AGE,
          uint64_t REALTIME_MAX_FILE_SIZE,
          TIME_SPAN REALTIME_MAX_FILE_AGE>
struct SimpleFinalizationPolicy {
  typedef TIMESTAMP T_TIMESTAMP;
  typedef TIME_SPAN T_TIME_SPAN;
  // This default strategy only supports MILLISECONDS from bricks:time as timestamps.
  bool ShouldFinalize(const QueueStatus<T_TIMESTAMP>& status, const T_TIMESTAMP now) const {
    if (status.appended_file_size >= BACKLOG_MAX_FILE_SIZE ||
        (now - status.appended_file_timestamp) > BACKLOG_MAX_FILE_AGE) {
      // Always keep files of at most 100KB and at most 24 hours old.
      return true;
    } else if (!status.finalized.queue.empty()) {
      // The above is the only condition as long as there are queued, pending, unprocessed files.
      return false;
    } else {
      // Otherwise, there are no files pending processing no queue,
      // and the default strategy can be legitimately expected to keep finalizing files somewhat often.
      return (status.appended_file_size >= REALTIME_MAX_FILE_SIZE ||
              (now - status.appended_file_timestamp) > REALTIME_MAX_FILE_AGE);
    }
  }
};

struct KeepFilesAround100KBUnlessNoBacklog
    : SimpleFinalizationPolicy<bricks::time::EPOCH_MILLISECONDS,
                               bricks::time::MILLISECONDS_INTERVAL,
                               100 * 1024,
                               bricks::time::MILLISECONDS_INTERVAL(24 * 60 * 60 * 1000),
                               10 * 1024,
                               bricks::time::MILLISECONDS_INTERVAL(10 * 60 * 1000)> {};

// Default file purge strategy: Keeps under 1K files of under 1GB of total volume.
template <uint64_t MAX_TOTAL_SIZE, size_t MAX_FILES>
struct SimplePurgePolicy {
  bool ShouldPurge(const QueueStatus<bricks::time::EPOCH_MILLISECONDS>& status) const {
    if (status.finalized.total_size + status.appended_file_size > MAX_TOTAL_SIZE) {
      // Purge the oldest queued files if the total size of data stored in the queue exceeds 1GB.
      return true;
    } else if (status.finalized.queue.size() > MAX_FILES) {
      // Purge the oldest queued files if the total number of queued files exceeds 1000.
      return true;
    } else {
      // Good to go otherwise.
      return false;
    }
  }
};

struct KeepUnder1GBAndUnder1KFiles : SimplePurgePolicy<1024 * 1024 * 1024, 1000> {};

// Default retry strategy for the processing of finalized files.
// On `Success`, processes files as they arrive without any delays.
// On `Unavaliable`, retries after an amount of time drawn from an exponential distribution
// with the mean defaulting to 15 minutes, min defaulting to 1 minute and max defaulting to 24 hours.
// ...
// Handles time skews correctly.
template <typename TIME_MANAGER_FOR_RETRY_STRATEGY, typename FILE_SYSTEM_FOR_RETRY_STRATEGY>
class RetryExponentially {
 public:
  typedef TIME_MANAGER_FOR_RETRY_STRATEGY T_TIME_MANAGER;
  typedef FILE_SYSTEM_FOR_RETRY_STRATEGY T_FILE_SYSTEM;
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
  void AttachToFile(const std::string /*filename*/) {
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

// Another simple file append strategy: Append messages adding a separator after each of them.
class AppendToFileWithSeparator {
 public:
  void AppendToFile(bricks::FileSystem::OutputFile& fo, const std::string& message) const {
    // TODO(dkorolev): Should we flush each record? Make it part of the strategy?
    fo << message << separator_ << std::flush;
  }
  uint64_t MessageSizeInBytes(const std::string& message) const {
    return message.length() + separator_.length();
  }
  void SetSeparator(const std::string& separator) {
    separator_ = separator;
  }

 private:
  std::string separator_ = "";
};

}  // namespace strategy
}  // namespace fsq

#endif  // FSQ_STRATEGIES_H
