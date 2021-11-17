#ifndef SUBPROCESS_REDIRECTION_H_
#define SUBPROCESS_REDIRECTION_H_
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include <filesystem>
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
    /// The stream is typically inherited from the parent. The field
    /// in `Popen` corresponding to the stream will be `std::nullopt`.
    struct None { };

    /// Redirect the stream to a pipe.
    ///
    /// This variant requests that a stream be redirected to a
    /// unidirectional pipe. One end of the pipe is passed to the
    /// child process and configured as one of its standard streams,
    /// and the other end is available to the parent for communicating
    /// with the child.
    ///
    /// The field with `Popen` corresponding to the stream will be
    /// an fdstream corresponding to the parent's end of the pipe.
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
    /// `PopenError::LogicError`.
    ///
    /// The field in `Popen` corresponding to the stream will be
    /// `std::nullopt`.
    struct Merge { };

    /// Redirect the stream to the specified open file descriptor.
    ///
    /// This does not create a pipe, it simply spawns the child so
    /// that the specified stream sees that file. The child can read
    /// from or write to the provided file on its own, without any
    /// intervention by the parent. Consider using `Redirection::Read`,
    /// `Redirection::Write`, or `Redirection::Append`, which are higher-
    /// level wrappers around this behavior.
    ///
    /// The field in `Popen` corresponding to the stream will be
    /// std::nullopt.
    struct FileDescriptor {
      int fd;
    private:
      bool _owned{false};

      void discard();
    public:
      FileDescriptor(int _fd);
      ~FileDescriptor();
      FileDescriptor(const FileDescriptor& other);

      FileDescriptor& operator=(const FileDescriptor& other);

      FileDescriptor(FileDescriptor&& other);
      FileDescriptor& operator=(FileDescriptor&& other);
    };

    /// Redirect the stream to/from the specified path, with other arguments as interpreted by open(2)
    ///
    /// You probably want one of Read, Write, or Append, which use this method
    /// behind the scenes but provide reasonable defaults for the flags and mode
    /// arguments.
    static Result<Redirection> Open(const std::filesystem::path& path, int flags, mode_t mode);

    /// Read the stream from the specified path.
    ///
    /// If no file exists at that path, or it cannot be opened, return a PopenError.
    /// This method does not read any bytes of the file, it just holds onto
    /// the file descriptor to be passed into Popen.
    static Result<Redirection> Read(const std::filesystem::path& path);

    /// Write the stream to the specified path.
    ///
    /// If a file already exists at that path it will be overwritten if we
    /// have permission. Otherwise a PopenError will be returned.
    /// This method does not write any bytes of the file, it just holds onto
    /// the file descriptor to be passed into Popen.
    static Result<Redirection> Write(const std::filesystem::path& path);

    /// Write the stream to the specified path.
    ///
    /// If a file already exists at that path it will be appended to, if we
    /// have permission. Otherwise a PopenError will be returned.
    /// This method does not write any bytes of the file, it just holds onto
    /// the file descriptor to be passed into Popen.
    static Result<Redirection> Append(const std::filesystem::path& path);

  private:
    using StateType = std::variant<None, Pipe, Merge, FileDescriptor>;
    StateType _state;
  public:
    template<typename... Args>
    Redirection(Args&&... args)
    : _state{std::forward<Args>(args)...}
    { }

    // Redirection(const Redirection& other)
    // : _state{other._state}
    // { }

    Redirection(Redirection&& other);

    // Redirection& operator=(const Redirection& other) {
    //   _state = other._state;
    //   return *this;
    // }
    Redirection& operator=(Redirection&& other);

    template <typename T>
    bool is_a() const {
      return std::holds_alternative<T>(_state);
    }
    template <typename T>
    T get() const {
      return std::get<T>(_state);
    }

    std::string toString() const;

    Result<const std::nullopt_t> match(
      std::function<Result<const std::nullopt_t>(const Pipe&)> pipe_case,
      std::function<Result<const std::nullopt_t>(const FileDescriptor&)> file_case,
      std::function<Result<const std::nullopt_t>(const Merge&)> merge_case,
      std::function<Result<const std::nullopt_t>()> none_case
    ) const;
  };
}  // namespace subprocess
#endif
