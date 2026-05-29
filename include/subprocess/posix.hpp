#ifndef SUBPROCESS_POSIX_H_
#define SUBPROCESS_POSIX_H_

#include <iostream>
#include <tuple>

#include "ExitStatus.hpp"
#include "Result.hpp"
#include "detail/platform.hpp"

namespace subprocess {

  /// Create an OS pipe.  Returns {read_fd, write_fd} on success.
  Result<std::tuple<int, int>> pipe();

  /// Set or clear the close-on-exec / non-inherit flag on a CRT file descriptor.
  void set_inheritable(int fd, bool heritable);

  void panic(std::string msg);

#ifndef _WIN32
  /// Decode a raw Unix waitpid(2) status word into an ExitStatus.
  ExitStatus decode_exit_status(int status);

  /// Reset SIGPIPE to its default disposition in the child after fork().
  int32_t reset_sigpipe();
#endif  // !_WIN32

}  // namespace subprocess
#endif