#ifndef SUBPROCESS_DETAIL_CHILD_STATE_H_
#define SUBPROCESS_DETAIL_CHILD_STATE_H_
#include <stdint.h>

#include <optional>
#include <string>
#include <variant>

#include "../ExitStatus.hpp"
#include "../Result.hpp"
#include "platform.hpp"

namespace subprocess {
  struct ChildState {
    struct Preparing { };
    struct Running {
      /// Platform process identifier (pid_t on Unix, DWORD on Windows).
      pid_type pid;  // NOLINT(misc-non-private-member-variables-in-classes)
#ifdef _WIN32
      /// Windows-only: open handle to the child process.
      /// Must be closed (via CloseHandle) when the process is reaped.
      void* process_handle;  // NOLINT(misc-non-private-member-variables-in-classes)
#endif
    };
    struct Finished {
      ExitStatus exit_status;  // NOLINT(misc-non-private-member-variables-in-classes)
    };

   private:
    using StateType = std::variant<Preparing, Running, Finished>;
    StateType _state;

   public:
    template<
        typename... Args, typename = std::enable_if_t<std::is_constructible_v<StateType, Args...>>>
    // NOLINTNEXTLINE(hicpp-explicit-conversions)
    ChildState(Args&&... args)
        : _state{ std::forward<Args>(args)... } { }

    ChildState(ChildState&& other) noexcept;
    ChildState(const ChildState& other);
    ChildState& operator=(ChildState&& other) noexcept;
    ChildState& operator=(const ChildState& other) = default;
    ~ChildState() = default;

    template<typename T>
    [[nodiscard]] bool is_a() const {
      return std::holds_alternative<T>(_state);
    }
    template<typename T>
    [[nodiscard]] T get() const {
      return std::get<T>(_state);
    }

    /// std::visit the internal variant with an arbitrary visitor.
    ///
    /// The visitor must be callable for every alternative (Preparing, Running,
    /// Finished).  The return type is deduced from the visitor.
    template<typename Visitor>
    [[nodiscard]] auto visit(Visitor&& visitor) const
        -> decltype(std::visit(std::forward<Visitor>(visitor), _state)) {
      return std::visit(std::forward<Visitor>(visitor), _state);
    }
  };
}  // namespace subprocess
#endif
