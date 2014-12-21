#ifndef FSQ_EXPONENTIAL_RETRY_STRATEGY_H
#define FSQ_EXPONENTIAL_RETRY_STRATEGY_H

#include <string>

// #include "../Bricks/util/util.h"
// #include "../Bricks/file/file.h"
#include "../Bricks/time/time.h"

namespace fsq {
namespace strategy {

// Exponential retry strategy for the processing of finalized files.
// On `Success`, processes files as they arrive without any delays.
// On `Unavaliable`, retries after an amount of time drawn from an exponential distribution
// with the mean defaulting to 15 minutes, min defaulting to 1 minute and max defaulting to 24 hours.
// ...
// Handles time skews correctly.
template <typename FILE_SYSTEM_FOR_RETRY_STRATEGY>
class ExponentialDelayRetryStrategy {
 public:
  typedef FILE_SYSTEM_FOR_RETRY_STRATEGY T_FILE_SYSTEM;
  struct DistributionParams {
    double mean, min, max;
    DistributionParams(double mean, double min, double max) : mean(mean), min(min), max(max) {
    }
    DistributionParams() = default;
    DistributionParams(const DistributionParams&) = default;
    DistributionParams& operator=(const DistributionParams&) = default;
  };
  explicit ExponentialDelayRetryStrategy(const T_FILE_SYSTEM& file_system, const DistributionParams& params)
      : file_system_(file_system),
        last_update_time_(bricks::time::Now()),
        time_to_be_ready_to_process_(last_update_time_),
        params_(params) {
    // TODO(dkorolev): Code it.
    // SetUpDistribution();
  }
  explicit ExponentialDelayRetryStrategy(const T_FILE_SYSTEM& file_system,
                                         const double mean = 15 * 60 * 1e3,
                                         const double min = 60 * 1e3,
                                         const double max = 24 * 60 * 60 * 1e3)
      : ExponentialDelayRetryStrategy(file_system, DistributionParams(mean, min, max)) {
  }
  void AttachToFile(const std::string /*filename*/) {
    // Serializes and deserializes itself into a file, used to preserve retry delays between restarts.
    // TODO(dkorolev): Support other means like CoreData, or stick with a file?
  }
  /*
  bool ReadyToProcess() const {
    const bricks::time::EPOCH_MILLISECONDS now = bricks::time::Now();
    if (now < last_update_time_) {
      // Possible time skew, stay on the safe side.
      last_update_time_ = now;
      time_to_be_ready_to_process_ = now;
      return true;
    } else {
      return now >= time_to_be_ready_to_process_;
    }
  }
  */
  // OnSuccess(): Clear all retry delays, cruising at full speed.
  void OnSuccess() {
    last_update_time_ = bricks::time::Now();
    time_to_be_ready_to_process_ = last_update_time_;
  }
  // OnFailure(): Set or update all retry delays.
  void OnFailure() {
    const bricks::time::EPOCH_MILLISECONDS now = bricks::time::Now();
    if (now < last_update_time_) {
      // Possible time skew, stay on the safe side.
      time_to_be_ready_to_process_ = now;
    }
    last_update_time_ = now;
    // TODO(dkorolev): Code it.
    ///    time_to_be_ready_to_process_ =
    ///        std::max(time_to_be_ready_to_process_, now + bricks::time::MILLISECONDS_INTERVAL(1000));  // now
    ///        + DrawFromExponentialDistribution();
  }
  bool ShouldWait(bricks::time::MILLISECONDS_INTERVAL* /*output_wait_ms*/) {
    return false;
  }

 private:
  const T_FILE_SYSTEM& file_system_;
  mutable typename bricks::time::EPOCH_MILLISECONDS last_update_time_;
  mutable typename bricks::time::EPOCH_MILLISECONDS time_to_be_ready_to_process_;
  const DistributionParams params_;
};

}  // namespace strategy
}  // namespace fsq

#endif  // FSQ_EXPONENTIAL_RETRY_STRATEGY_H
