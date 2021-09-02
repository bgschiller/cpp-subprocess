#ifndef SUBPROCESS_EXIT_STATUS_H_
#define SUBPROCESS_EXIT_STATUS_H_
#include <stdint.h>

#include <string>
#include <variant>
namespace subprocess {

  struct ExitStatus {
    /**
     * The process exited with the specified exit code.
     *
     * Note that the exit code is limited to a much smaller range on most platforms.
     */
    struct Exited {
      int32_t code;

      std::string toString() const;
    };

    /**
     * The process exited due to a signal with the specified number.
     *
     * The variant is never created on Windows, where signals of Unix kind do not exist.
     */
    struct Signaled {
      int32_t signal;

      std::string toString() const;
    };
    /**
     * The process exit status cannot be described by the preceding two variants.
     *
     * This should not occur in normal operation.
     */
    struct Other {
      int32_t code;

      std::string toString() const;
    };

    /**
     * It is known that the process has completed, but its exit status is unavailable.
     *
     * This should not occur in normal operation but is possible if, for example, some foreign code
     * calls `waitpid()` on the PID of the child process.
     */
    struct Undetermined { };
   private:
    using StateType = std::variant<Exited, Signaled, Other, Undetermined>;
    StateType _state;
   public:

    template<typename... Args>
    ExitStatus(Args&&... args)
    : _state{std::forward<Args>(args)...}
    { }
    ExitStatus(const ExitStatus& other);
    ExitStatus(ExitStatus&& other);

    bool success() const;

    std::string toString() const;

    ExitStatus& operator=(ExitStatus&& other);

  };
}  // namespace subprocess
#endif
