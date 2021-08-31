#ifndef SUBPROCESS_CHILD_STATE_H_
#define SUBPROCESS_CHILD_STATE_H_
#include <stdint.h>

#include <fstream>
#include <functional>
#include <optional>
#include <variant>

#include "ExitStatus.hpp"
#include "Result.hpp"
namespace subprocess {
  struct ChildState {
    struct Preparing { };
    struct Running {
      pid_t pid;
    };
    struct Finished {
      ExitStatus exit_status;
    };
   private:
    using StateType =  std::variant<Preparing, Running, Finished>;
    const StateType _state;
   public:

    template<typename... Args>
    ChildState(Args&&... args)
    : _state{std::forward<Args>(args)...}
    { }

    template <typename T>
    bool is_a() const {
      return std::holds_alternative<T>(_state);
    }
    template <typename T>
    T get() const {
      return std::get<T>(_state);
    }

    Result<const std::nullopt_t> match(
      std::function<Result<const std::nullopt_t>(const Preparing&)> preparing_case,
      std::function<Result<const std::nullopt_t>(const Running&)> running_case,
      std::function<Result<const std::nullopt_t>(const Finished&)> finished_case
    ) const;

  };
}  // namespace subprocess
#endif
