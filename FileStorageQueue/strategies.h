#ifndef FSQ_STRATEGIES_H
#define FSQ_STRATEGIES_H

#include <string>

#include "../Bricks/util/util.h"
#include "../Bricks/file/file.h"
#include "../Bricks/time/time.h"
#include "../Bricks/strings/fixed_size_serializer.h"

namespace fsq {
namespace strategy {

// Default file naming strategy.
// ***** !!!!! *****
// TODO(dkorolev): Tweak it.
// ***** !!!!! *****
const char kFinalizedPrefix[] = "finalized-";
const char kFinalizedSuffix[] = ".bin";
const size_t kFinalizedPrefixLength = bricks::CompileTimeStringLength(kFinalizedPrefix);
const size_t kFinalizedSuffixLength = bricks::CompileTimeStringLength(kFinalizedSuffix);

struct DummyFileNamingToUnblockAlexFromMinsk {
  template <typename T_TIMESTAMP>
  inline static std::string GenerateCurrentFileName(const T_TIMESTAMP timestamp) {
    return "current-" + bricks::strings::PackToString(timestamp) + ".bin";
  }
  template <typename T_TIMESTAMP>
  inline static std::string GenerateFinalizedFileName(const T_TIMESTAMP timestamp) {
    return kFinalizedPrefix + bricks::strings::PackToString(timestamp) + kFinalizedSuffix;
  }
  template <typename T_TIMESTAMP>
  inline static bool IsFinalizedFileName(const std::string& filename) {
    // TODO(dkorolev): Get a bit smarter here.
    return (filename.length() ==
            kFinalizedPrefixLength + bricks::strings::FixedSizeSerializer<T_TIMESTAMP>::size_in_bytes +
                kFinalizedSuffixLength) &&
           filename.substr(0, kFinalizedPrefixLength) == kFinalizedPrefix;
  }
};

// Default retry strategy for file processing.
// On success, runs at full speed without any delays.
// On failure, retries after an amount of time drawn from an exponential distribution
// with the mean defaulting to 15 minutes, min defaulting to 1 minute and max defaulting to 24 hours.
// On forced retry and failure updates the delay keeping the max of { current, newly suggested }.
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

// Default file finalization strategy.
// Does what the name says: Keeps files around 100KB, unless there is no backlog,
// in which case files are finalized sooner for reduced processing latency.
struct KeepFilesAround100KBUnlessNoBacklog {
  typedef bricks::time::UNIX_TIME_MILLISECONDS ABSOLUTE_MS;
  typedef bricks::time::MILLISECONDS_INTERVAL DELTA_MS;
  // The default implementation only supports MILLISECOND as timestamps.
  bool ShouldFinalize(const QueueStatus<ABSOLUTE_MS, DELTA_MS>& status) const {
    if (status.appended_file_size >= 100 * 1024 * 1024 ||
        status.appended_file_age > DELTA_MS(24 * 60 * 60 * 1000)) {
      // Always keep files of at most 100KB and at most 24 hours old.
      return true;
    } else if (status.number_of_queued_files > 0) {
      // The above is the only condition as long as there are queued, pending, unprocessed files.
      return false;
    } else {
      // Otherwise, there are no files pending processing no queue,
      // and the default strategy can be legitimately expected to keep finalizing files somewhat often.
      return (status.appended_file_size >= 10 * 1024 * 1024 ||
              status.appended_file_age > DELTA_MS(10 * 60 * 1000));
    }
  }
};

// Default file purge strategy.
// Does what the name says: Keeps under 1'000 files of under 1GB total volume.
struct KeepUnder1GBAndUnder1KFiles {
  bool ShouldPurge(const QueueStatus<bricks::time::UNIX_TIME_MILLISECONDS, bricks::time::MILLISECONDS_INTERVAL>&
                       status) const {
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

// Default file append strategy.
// Appends data to files in raw format, without separators.
struct JustAppendToFile {
  void AppendToFile(bricks::FileSystem::OutputFile& fo, const std::string& message) const {
    // TODO(dkorolev): Should we flush each record? Make it part of the strategy?
    fo << message << std::flush;
  }
  uint64_t MessageSizeInBytes(const std::string& message) const {
    return message.length();
  }
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

// Default time strategy: Use UNIX time in milliseconds.
struct UseUNIXTimeInMilliseconds final {
  typedef bricks::time::UNIX_TIME_MILLISECONDS T_TIMESTAMP;
  typedef bricks::time::MILLISECONDS_INTERVAL T_TIME_SPAN;
  T_TIMESTAMP Now() const {
    return bricks::time::Now();
  }
};

}  // namespace strategy
}  // namespace fsq

#endif  // FSQ_STRATEGIES_H
