#ifndef SUBPROCESS_REDIRECTION_H_
#define SUBPROCESS_REDIRECTION_H_
#include <stdint.h>

#include <fstream>
#include <functional>
#include <optional>
#include <variant>

#include "subprocess/variant_helpers.hpp"
#include "subprocess/Result.hpp"

namespace subprocess {

  struct Redirection {
    /// Do nothing with the stream.
    ///
    /// The stream is typically inherited from the parent.  The field
    /// in `Popen` corresponding to the stream will be `None`.
    struct None { };

    /// Redirect the stream to a pipe.
    ///
    /// This variant requests that a stream be redirected to a
    /// unidirectional pipe.  One end of the pipe is passed to the
    /// child process and configured as one of its standard streams,
    /// and the other end is available to the parent for communicating
    /// with the child.
    ///
    /// The field with `Popen` corresponding to the stream will be
    /// `Some(file)`, `File` being the parent's end of the pipe.
    struct Pipe { };

    /// Merge the stream to the other output stream.
    ///
    /// This variant is only valid when configuring redirection of
    /// standard output and standard error.  Using
    /// `Redirection::Merge` for `PopenConfig::stderr` requests the
    /// child's stderr to refer to the same underlying file as the
    /// child's stdout (which may or may not itself be redirected),
    /// equivalent to the `2>&1` operator of the Bourne shell.
    /// Analogously, using `Redirection::Merge` for
    /// `PopenConfig::stdout` is equivalent to `1>&2` in the shell.
    ///
    /// Specifying `Redirection::Merge` for `PopenConfig::stdin` or
    /// specifying it for both `stdout` and `stderr` is invalid and
    /// will cause `Popen::create` to return
    /// `Err(PopenError::LogicError)`.
    ///
    /// The field in `Popen` corresponding to the stream will be
    /// `None`.
    struct Merge { };

    /// Redirect the stream to the specified open `File`.
    ///
    /// This does not create a pipe, it simply spawns the child so
    /// that the specified stream sees that file.  The child can read
    /// from or write to the provided file on its own, without any
    /// intervention by the parent.
    ///
    /// The field in `Popen` corresponding to the stream will be
    /// `None`.
    struct File {
      int fd;
    };
  private:
    using StateType = std::variant<None, Pipe, Merge, File>;
    StateType _state;
  public:
    template<typename... Args>
    Redirection(Args&&... args)
    : _state{std::forward<Args>(args)...}
    { }

    template <typename T>
    bool is_a() const {
      return std::holds_alternative<T>(_state);
    }
    template <typename T>
    T get() const {
      return std::get<T>(_state);
    }

    std::string toString() {
      return internal::variant_to_string(_state);
    }

    Result<const std::nullopt_t> match(
      std::function<Result<const std::nullopt_t>(const Pipe&)> pipe_case,
      std::function<Result<const std::nullopt_t>(const File&)> file_case,
      std::function<Result<const std::nullopt_t>(const Merge&)> merge_case,
      std::function<Result<const std::nullopt_t>()> none_case
    ) const;
  };
}  // namespace subprocess
#endif
