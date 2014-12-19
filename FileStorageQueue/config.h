#ifndef FSQ_CONFIG_H
#define FSQ_CONFIG_H

#include <string>

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
  typedef strategy::KeepUnder1GBAndUnder1KFiles T_PURGE_POLICY;
  template <class TIME_MANAGER, class FILE_SYSTEM>
  using T_RETRY_POLICY = strategy::RetryExponentially<TIME_MANAGER, FILE_SYSTEM>;
  static bool DetachProcessingThreadOnTermination() {
    return false;
  }
};

}  // namespace fsq

#endif  // FSQ_CONFIG_H
