#ifndef SANDBOX_CLIENT_FILE_STORAGE_FLAGS_H
#define SANDBOX_CLIENT_FILE_STORAGE_FLAGS_H

#include <type_traits>

#include "client_file_storage_types.h"

#include "../Bricks/dflags/dflags.h"

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

// Since `class ClientFileStorage` is timestamp-type-agnostic, while flags assume milliseconds,
// only support instances of ClientFileStorage that use `bricks::time::UNIX_TIME_MILLISECONDS` as T_TIMESTAMP.
template <class PROCESSOR,
          template <class TIME_MANAGER, class FILE_SYSTEM> class RETRY_POLICY,
          class FINALIZE_POLICY,
          class PURGE_POLICY,
          typename MESSAGE,
          class FILE_APPEND_POLICY,
          class TIME_MANAGER,
          class FILE_SYSTEM>
struct ClientFileStorageParamsFromFlags {
  typedef ClientFileStorageParams<PROCESSOR,
                                  RETRY_POLICY,
                                  FINALIZE_POLICY,
                                  PURGE_POLICY,
                                  MESSAGE,
                                  FILE_APPEND_POLICY,
                                  TIME_MANAGER,
                                  FILE_SYSTEM> Params;
  static typename std::enable_if<
      std::is_same<typename TIME_MANAGER::T_TIMESTAMP, bricks::time::UNIX_TIME_MILLISECONDS>::value,
      Params>::type
  Construct() {
    return Params()
        .set_current_filename(FLAGS_current_filename)
        .set_committed_filename(FLAGS_committed_filename)
        .set_max_file_age(FLAGS_max_file_age_ms)
        .set_max_file_size(FLAGS_max_file_size);
  }
};

#endif  // SANDBOX_CLIENT_FILE_STORAGE_FLAGS_H
