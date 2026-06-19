#ifndef SUBPROCESS_POPEN_ERROR_H_
#define SUBPROCESS_POPEN_ERROR_H_

#include <string>
#include <utility>

namespace subprocess {

  struct PopenError {
    enum class ErrKind : uint8_t { IoError, LogicError };
    constexpr static ErrKind IoError = ErrKind::IoError;
    constexpr static ErrKind LogicError = ErrKind::LogicError;
    ErrKind kind;         // NOLINT(misc-non-private-member-variables-in-classes)
    std::string message;  // NOLINT(misc-non-private-member-variables-in-classes)

    PopenError(ErrKind error_kind, const std::string& error_message);
    PopenError(PopenError&& other) noexcept;
    PopenError(const PopenError& other);
    PopenError& operator=(PopenError&& other) noexcept;
    PopenError& operator=(const PopenError& other);
    ~PopenError() = default;
  };
}  // namespace subprocess
#endif
