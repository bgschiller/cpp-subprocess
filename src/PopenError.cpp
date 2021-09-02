#include "subprocess/PopenError.hpp"

using namespace PopenError;

PopenError::PopenError(ErrKind _kind, const std::string& _message)
: kind{_kind}
, message{_message}
{ }

PopenError::PopenError(PopenError&& other)
: kind{other.kind}
, message{std::move(other.message)}
{ }

PopenError::PopenError(const PopenError& other)
: kind{other.kind}
, message{other.message}
{ }

PopenError& PopenError::operator=(PopenError&& other) {
  kind = other.kind;
  message = std::move(other.message);
  return *this;
}
