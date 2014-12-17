#ifndef SANDBOX_CLIENT_FILE_STORAGE_TYPES_H
#define SANDBOX_CLIENT_FILE_STORAGE_TYPES_H

#include <string>

// The status of ClientFileStorage file queue.
template <typename TIME_SPAN>
struct QueueStatus {
  typedef TIME_SPAN T_TIME_SPAN;

  uint64_t appended_file_size;
  T_TIME_SPAN appended_file_age;

  size_t number_of_queued_files;
  uint64_t total_queued_files_size;
  T_TIME_SPAN oldest_queued_file_age;
};

// Parameters for ClientFileStorage.
template <class CONFIG>
struct ClientFileStorageParams {
#ifdef PARAM
#error "'PARAM' should not be defined by this point."
#else
#define PARAM(type, param)                           \
  type param;                                        \
  ClientFileStorageParams& set_##param(type value) { \
    param = value;                                   \
    return *this;                                    \
  }
  PARAM(std::string, current_filename);
  PARAM(std::string, committed_filename);  // TODO(dkorolev): Rename this.
  PARAM(typename CONFIG::TIME_MANAGER::T_TIMESTAMP, max_file_age);
  PARAM(uint64_t, max_file_size);
#undef PARAM
#endif
};

#endif  // SANDBOX_CLIENT_FILE_STORAGE_PARAMS_H
