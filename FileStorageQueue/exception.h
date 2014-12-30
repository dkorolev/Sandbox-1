#ifndef FSQ_EXCEPTION_H
#define FSQ_EXCEPTION_H

#include <iostream>

namespace fsq {

// Exception type, only defined if not disabled by command-line flags.
#ifndef ALEX_FROM_MINSK_NO_EXCEPTIONS
struct FSQException : std::exception {
  // TODO(dkorolev): Fill this class.
};
#endif  // ALEX_FROM_MINSK_NO_EXCEPTIONS

// Error handling, to suppoty both exception-friendly and exception-free implementations.
namespace strategy {
struct DefaultErrorHandling {
  static void HandleError() {
#ifndef ALEX_FROM_MINSK_NO_EXCEPTIONS
    throw FSQException();
#else
    std::cerr << "EXCEPTION!\n";
#endif
  }
};

}  // namespace strategy

}  // namespace fsq

#endif  // FSQ_EXCEPTION_H
