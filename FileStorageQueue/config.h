#ifndef FSQ_CONFIG_H
#define FSQ_CONFIG_H

#include <string>

#include "../Bricks/time/time.h"

#include "strategies.h"

namespace fsq {

// Default strategy configuration for FSQ.
//
// The easiest way to create a user-specific configuration is to derive from this class
// and alter certain fields.
//
// FSQ derives itself from all strategy classes except T_PROCESSOR, T_TIME_MANAGER and T_FILE_MANAGER,
// thus allowing calling member setters for other policies directly on itself.

template <typename PROCESSOR>
struct Config {
  typedef PROCESSOR T_PROCESSOR;
  typedef std::string T_MESSAGE;
  typedef strategy::JustAppendToFile T_FILE_APPEND_POLICY;
  typedef strategy::DummyFileNamingToUnblockAlexFromMinsk T_FILE_NAMING_STRATEGY;
  typedef bricks::FileSystem T_FILE_SYSTEM;
  typedef strategy::UseUNIXTimeInMilliseconds T_TIME_MANAGER;
  typedef strategy::KeepFilesAround100KBUnlessNoBacklog T_FINALIZE_POLICY;
  typedef strategy::KeepUnder20MBAndUnder1KFiles T_PURGE_POLICY;
  template <class TIME_MANAGER, class FILE_SYSTEM>
  using T_RETRY_POLICY = strategy::RetryExponentially<TIME_MANAGER, FILE_SYSTEM>;

  // Set to true to have FSQ detach the processing thread instead of joining it in destructor.
  static bool DetachProcessingThreadOnTermination() {
    return false;
  }

  // Set to false to have PushMessage() throw an exception when attempted to push while shutting down.
  static bool NoThrowOnPushMessageWhileShuttingDown() {
    return true;
  }

  // Set to true to have FSQ process all queued files in destructor before returning.
  static bool ProcessQueueToTheEndOnShutdown() {
    return false;
  }

  template <typename T_FSQ_INSTANCE>
  static void Initialize(T_FSQ_INSTANCE&) {
    // `T_CONFIG::Initialize(*this)` is invoked from FSQ's constructor
    // User-derived Config-s can put non-static initialization code here.
  }
};

}  // namespace fsq

#endif  // FSQ_CONFIG_H
