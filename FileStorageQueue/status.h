// The status of FSQ's file system usage.

#ifndef FSQ_STATUS_H
#define FSQ_STATUS_H

#include <deque>
#include <string>
#include <tuple>

namespace fsq {

template <typename TIMESTAMP>
struct FileInfo {
  typedef TIMESTAMP T_TIMESTAMP;
  std::string name = std::string("");
  T_TIMESTAMP timestamp = T_TIMESTAMP(0);
  uint64_t size = 0;
  FileInfo(const std::string& name, T_TIMESTAMP timestamp, uint64_t size)
      : name(name), timestamp(timestamp), size(size) {
  }
  inline bool operator<(const FileInfo& rhs) const {
    return std::tie(timestamp, name) < std::tie(rhs.timestamp, rhs.name);
  }
};

// The status of all other, finalized, files combined.
template <typename TIMESTAMP>
struct QueueFinalizedFilesStatus {
  typedef TIMESTAMP T_TIMESTAMP;
  std::deque<FileInfo<T_TIMESTAMP>> queue;  // Sorted from oldest to newest.
  uint64_t total_size = 0;
};

// The status of the file that is currently being appended to.
template <typename TIMESTAMP>
struct QueueStatus {
  typedef TIMESTAMP T_TIMESTAMP;
  uint64_t appended_file_size = 0;                       // Also zero if no file is currently open.
  T_TIMESTAMP appended_file_timestamp = T_TIMESTAMP(0);  // Also zero if no file is curently open.
  QueueFinalizedFilesStatus<T_TIMESTAMP> finalized;
};

}  // namespace fsq

#endif  // FSQ_STATUS_H
