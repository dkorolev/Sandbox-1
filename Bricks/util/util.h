#ifndef BRICKS_UTIL_UTIL_H
#define BRICKS_UTIL_UTIL_H

#include <cstddef>

namespace bricks {

template <size_t N>
constexpr size_t CompileTimeStringLength(char const (&)[N]) {
  return N - 1;
}

template <typename T>
class ReadOnlyByConstRefFieldAccessor final {
 public:
  explicit ReadOnlyByConstRefFieldAccessor(const T& ref) : ref_(ref) {
  }
  inline operator const T&() const {
    return ref_;
  }

 private:
  ReadOnlyByConstRefFieldAccessor() = delete;
  const T& ref_;
};

}  // namespace bricks

#endif  // BRICKS_UTIL_UTIL_H
