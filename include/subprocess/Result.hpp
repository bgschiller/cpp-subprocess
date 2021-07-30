#ifndef SUBPROCESS_EXIT_STATUS_H_
#define SUBPROCESS_EXIT_STATUS_H_

#include <string>
#include <variant>

namespace subprocess {
  template<class T>
  class Result {
    using StateType = std::variant<PopenError, T>;
    StateType _state;
    Result(StateType&& state)
    : _state{std::move(state)}
    { }

    bool ok() const {
      return std::holds_alternative<T>(_state);
    }
    PopenError&& take_error() {
      return std::move(std::get<PopenError>(_state));
    }
    T&& take_value() {
      return std::move(std::get<T>(_state));
    }
  };
}

#endif