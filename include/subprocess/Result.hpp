#ifndef SUBPROCESS_RESULT_H_
#define SUBPROCESS_RESULT_H_

#include <string>
#include <variant>

#include "PopenError.hpp"

namespace subprocess {
  template<class T>
  class Result {
    using StateType = std::variant<PopenError, T>;
    StateType _state;

  public:
    template<typename... Args>
    Result(Args&&... args)
    : _state{std::forward<Args>(args)...}
    { }

    Result& operator=(Result&& other) {
      _state = std::move(other._state);
      return *this;
    }

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