#ifndef SUBPROCESS_POSIX_H_
#define SUBPROCESS_POSIX_H_

#include <iostream>
#include <string>
#include <tuple>

#include "ExitStatus.hpp"
#include "Result.hpp"
#include "detail/platform.hpp"

namespace subprocess {

  /// Create an OS pipe.  Returns {read_fd, write_fd} on success.
  Result<std::tuple<int, int>> pipe();

  /// Set or clear the close-on-exec / non-inherit flag on a CRT file descriptor.
  void set_inheritable(int fd, bool heritable);

  /// Close a CRT file descriptor (portable wrapper around close()/_close()).
  int close_fd(int fd);

  /// Duplicate a CRT file descriptor (portable wrapper around dup()/_dup()).
  int dup_fd(int fd);

  /// Portable, thread-safe translation of an errno value to a message string.
  std::string error_string(int errnum);

  void panic(std::string msg);

#ifndef _WIN32
  /// Decode a raw Unix waitpid(2) status word into an ExitStatus.
  ExitStatus decode_exit_status(int status);

  /// Reset SIGPIPE to its default disposition in the child after fork().
  int32_t reset_sigpipe();
#endif  // !_WIN32

}  // namespace subprocess
#endif