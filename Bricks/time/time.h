#ifndef BRICKS_TIME_TIME_H
#define BRICKS_TIME_TIME_H

#include "../strings/fixed_size_serializer.h"
// TODO(dkorolev): Add platform-dependent tests comparing Bricks time to UNIX time.

namespace bricks {

namespace time {

enum class EPOCH_MILLISECONDS : uint64_t {};
enum class MILLISECONDS_INTERVAL : uint64_t {};

inline EPOCH_MILLISECONDS Now() {
  return static_cast<EPOCH_MILLISECONDS>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                             std::chrono::system_clock::now().time_since_epoch()).count());
}

}  // namespace time

namespace strings {

template <>
struct FixedSizeSerializer<bricks::time::EPOCH_MILLISECONDS> {
  enum { size_in_bytes = std::numeric_limits<uint64_t>::digits10 + 1 };
  static std::string PackToString(bricks::time::EPOCH_MILLISECONDS x) {
    std::ostringstream os;
    os << std::setfill('0') << std::setw(size_in_bytes) << static_cast<uint64_t>(x);
    return os.str();
  }
  static bricks::time::EPOCH_MILLISECONDS UnpackFromString(std::string const& s) {
    uint64_t x;
    std::istringstream is(s);
    is >> x;
    return static_cast<bricks::time::EPOCH_MILLISECONDS>(x);
  }
};

}  // namespace strings

}  // namespace bricks

inline bricks::time::MILLISECONDS_INTERVAL operator-(bricks::time::EPOCH_MILLISECONDS lhs,
                                                     bricks::time::EPOCH_MILLISECONDS rhs) {
  return static_cast<bricks::time::MILLISECONDS_INTERVAL>(static_cast<int64_t>(lhs) -
                                                          static_cast<int64_t>(rhs));
}

// TODO(dkorolev): Add more arithmetic operations on milliseconds here.

#endif
