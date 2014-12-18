#ifndef FSQ_TYPES_H
#define FSQ_TYPES_H

#include <string>

namespace fsq {

// The status of FSQ file queue.
template <typename TIME_SPAN>
struct QueueStatus {
  typedef TIME_SPAN T_TIME_SPAN;

  uint64_t appended_file_size;
  T_TIME_SPAN appended_file_age;

  size_t number_of_queued_files;
  uint64_t total_queued_files_size;
  T_TIME_SPAN oldest_queued_file_age;
};

// Parameters for FSQ.
template <class CONFIG>
struct FSQParams {
#ifdef PARAM
#error "'PARAM' should not be defined by this point."
#else
#define PARAM(type, param)             \
  type param;                          \
  FSQParams& set_##param(type value) { \
    param = value;                     \
    return *this;                      \
  }
  PARAM(std::string, current_filename);
  PARAM(std::string, committed_filename);
#undef PARAM
#endif
};

}  // namespace fsq

#endif  // FSQ_TYPES_H
