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
      int32_t code;  // NOLINT(misc-non-private-member-variables-in-classes)

      [[nodiscard]] std::string toString() const;
    };

    /**
     * The process exited due to a signal with the specified number.
     *
     * The variant is never created on Windows, where signals of Unix kind do not exist.
     */
    struct Signaled {
      int32_t signal;  // NOLINT(misc-non-private-member-variables-in-classes)

      [[nodiscard]] std::string toString() const;
    };
    /**
     * The process exit status cannot be described by the preceding two variants.
     *
     * This should not occur in normal operation.
     */
    struct Other {
      int32_t code;  // NOLINT(misc-non-private-member-variables-in-classes)

      [[nodiscard]] std::string toString() const;
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
    template<
        typename... Args, typename = std::enable_if_t<std::is_constructible_v<StateType, Args...>>>
    // NOLINTNEXTLINE(hicpp-explicit-conversions)
    ExitStatus(Args&&... args)
        : _state{ std::forward<Args>(args)... } { }

    template<typename T>
    [[nodiscard]] bool is_a() const {
      return std::holds_alternative<T>(_state);
    }
    template<typename T>
    [[nodiscard]] T get() const {
      return std::get<T>(_state);
    }
    ExitStatus(const ExitStatus& other) = default;
    ExitStatus(ExitStatus&& other) noexcept;
    ExitStatus& operator=(const ExitStatus& other) = default;
    ExitStatus& operator=(ExitStatus&& other) noexcept;
    ~ExitStatus() = default;

    [[nodiscard]] bool success() const;

    [[nodiscard]] std::string toString() const;

    /// std::visit the internal variant with an arbitrary visitor.
    ///
    /// The visitor must be callable for every alternative (Exited, Signaled,
    /// Other, Undetermined).  The return type is deduced from the visitor.
    template<typename Visitor>
    [[nodiscard]] auto visit(Visitor&& visitor) const
        -> decltype(std::visit(std::forward<Visitor>(visitor), _state)) {
      return std::visit(std::forward<Visitor>(visitor), _state);
    }
  };
}  // namespace subprocess
#endif
