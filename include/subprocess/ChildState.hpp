#ifndef SUBPROCESS_CHILD_STATE_H_
#define SUBPROCESS_CHILD_STATE_H_
#include <stdint.h>

#include <fstream>
#include <variant>

#include "ExitStatus.hpp"
namespace subprocess {
  struct ChildState {

    struct Preparing { };
    struct Running {
      pid_t pid;
    };
    struct Finished {
      ExitStatus exit_status;
    };

    using StateType =  std::variant<internal::Preparing, internal::Running, internal::Finished>;

    ChildState(StateType&& state)
      : _state{std::move(state)}
      { }
    private:
    const StateType _state;
  };
}  // namespace subprocess
#endif
