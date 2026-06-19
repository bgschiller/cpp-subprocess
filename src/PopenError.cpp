#include "subprocess/PopenError.hpp"

#include <utility>

using namespace subprocess;

PopenError::PopenError(ErrKind error_kind, const std::string& error_message)
    : kind{ error_kind }
    , message{ error_message } { }

PopenError::PopenError(PopenError&& other) noexcept
    : kind{ other.kind }
    , message{ std::move(other.message) } { }

PopenError::PopenError(const PopenError& other) = default;

PopenError& PopenError::operator=(PopenError&& other) noexcept {
  kind = other.kind;
  message = std::move(other.message);
  return *this;
}

PopenError& PopenError::operator=(const PopenError& other) {
  if (this != &other) {
    kind = other.kind;
    message = other.message;
  }
  return *this;
}
