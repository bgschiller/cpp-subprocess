#ifndef SUBPROCESS_EXIT_STATUS_H_
#define SUBPROCESS_EXIT_STATUS_H_
#include <stdint.h>

#include <string>
#include <variant>
namespace subprocess {
  /**
   * The process exited with the specified exit code.
   *
   * Note that the exit code is limited to a much smaller range on most platforms.
   */

  namespace internal {
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
  }  // namespace internal
  class ExitStatus
      : public std::variant<
            internal::Exited, internal::Signaled, internal::Other, internal::Undetermined> {
   public:
    using variant::variant;
    using Exited = internal::Exited;
    using Signaled = internal::Signaled;
    using Other = internal::Other;
    using Undetermined = internal::Undetermined;

    bool success() const;

    std::string toString() const;
  };
}  // namespace subprocess
#endif
