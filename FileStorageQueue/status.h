// The status of FSQ's file system usage.

#ifndef FSQ_STATUS_H
#define FSQ_STATUS_H

namespace fsq {

// The status of all other, finalized, files combined.
template <typename TIMESTAMP, typename TIME_SPAN>
struct QueueFinalizedFilesStatus {
  typedef TIMESTAMP T_TIMESTAMP;
  typedef TIME_SPAN T_TIME_SPAN;
  size_t number_of_queued_files = 0;
  uint64_t total_queued_files_size = 0;
  std::string oldest_queued_file_name = std::string("");  // The file to be passed to the processor next.
  T_TIMESTAMP oldest_queued_file_timestamp = T_TIMESTAMP(0);
  T_TIME_SPAN oldest_queued_file_age = T_TIME_SPAN(0);
  uint64_t oldest_queued_file_size = 0;
  void UpdateFinalizedFilesStatus(const QueueFinalizedFilesStatus& rhs) {
    *this = rhs;
  }
};

// The status of the file that is currently being appended to.
template <typename TIMESTAMP, typename TIME_SPAN>
struct QueueStatus : QueueFinalizedFilesStatus<TIMESTAMP, TIME_SPAN> {
  typedef TIMESTAMP T_TIMESTAMP;
  typedef TIME_SPAN T_TIME_SPAN;
  uint64_t appended_file_size = 0;                       // Also zero if no file is currently open.
  T_TIMESTAMP appended_file_timestamp = T_TIMESTAMP(0);  // Also zero if no file is curently open.
  T_TIME_SPAN appended_file_age = T_TIME_SPAN(0);        // Also zero if no file is currently open.
};

}  // namespace fsq

#endif  // FSQ_STATUS_H
