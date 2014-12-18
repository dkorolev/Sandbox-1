#ifndef FSQ_FLAGS_H
#define FSQ_FLAGS_H

#include <type_traits>

#include "fsq_types.h"

#include "../Bricks/dflags/dflags.h"

DEFINE_string(current_filename,
              "current",
              "The name of the file to be appended to.");  // TODO(dkorolev): Timestamp it.

DEFINE_string(committed_filename,
              "done",
              "The name of the file to rename completed files into.");  // TODO(dkorolev): Timestamp them.

namespace fsq {

template <class CONFIG>
struct FSQParamsFromFlags {
  typedef CONFIG T_CONFIG;
  typedef FSQParams<CONFIG> params_type;
  params_type Construct() {
    return params_type().set_current_filename(FLAGS_current_filename).set_committed_filename(
        FLAGS_committed_filename);
  }
};

}  // namespace fsq

#endif  // FSQ_FLAGS_H
