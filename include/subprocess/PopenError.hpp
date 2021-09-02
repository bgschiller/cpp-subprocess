#ifndef SUBPROCESS_POPEN_ERROR_H_
#define SUBPROCESS_POPEN_ERROR_H_
#include <variant>
namespace subprocess {

  struct PopenError {
    enum class ErrKind { IoError, LogicError };
    constexpr static ErrKind IoError = ErrKind::IoError;
    constexpr static ErrKind LogicError = ErrKind::LogicError;
    const ErrKind kind;
    const std::string message;

    PopenError(ErrKind _kind, const std::string& _message);
    PopenError(PopenError&& other);
    PopenError(const PopenError& other);
    PopenError& operator=(PopenError&& other);
  };
}  // namespace subprocess
#endif
