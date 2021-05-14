#ifndef SUBPROCESS_CHILD_STATE_H_
#define SUBPROCESS_CHILD_STATE_H_
#include <stdint.h>

#include <fstream>
#include <variant>

#include "ExitStatus.hpp"
namespace subprocess {

  namespace internal {
    struct Preparing { };
    struct Running {
      pid_t pid;
    };
    struct Finished {
      ExitStatus exit_status;
    };
  }  // namespace internal

  struct ChildState
      : public std::variant<internal::Preparing, internal::Running, internal::Finished> {
    using Preparing = internal::Preparing;
    using Running = internal::Running;
    using Finished = internal::Finished;
  };
}  // namespace subprocess
#endif
