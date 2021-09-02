#include "subprocess/posix.hpp"

#include <unistd.h>
#include <string.h>

namespace subprocess {


Result<std::tuple<int, int>> pipe() {
  int pipe_fds[2];
  if (::pipe(pipe_fds) != 0) {
    return PopenError{PopenError::ErrKind::IoError, std::string("pipe(): ") + std::to_string(errno) + std::string(" ") + strerror(errno)};
  }
  return std::make_tuple(pipe_fds[0], pipe_fds[1]);
}

void set_inheritable(int fd, bool heritable) {
  int curr = fcntl(fd, F_GETFD);
  fcntl(fd, F_SETFD, heritable ? (curr & ~FD_CLOEXEC) : (curr | FD_CLOEXEC));
}

ExitStatus decode_exit_status(int status) {
  if (WIFEXITED(status)) {
    return ExitStatus::Exited{WEXITSTATUS(status)};
  } else if (WIFSIGNALED(status)) {
    return ExitStatus::Signaled{WTERMSIG(status)};
  } else {
    return ExitStatus::Other{status};
  }
}

void panic(std::string msg) {
  std::cerr << msg << std::endl;
  std::exit(1);
}

int32_t reset_sigpipe() {
  // This is called after forking to reset SIGPIPE handling to the
  // defaults that Unix programs expect.  Quoting
  // std::process::Command::do_exec:
  //
  // """
  // libstd ignores SIGPIPE, and signal-handling libraries often set
  // a mask. Child processes inherit ignored signals and the signal
  // mask from their parent, but most UNIX programs do not reset
  // these things on their own, so we need to clean things up now to
  // avoid confusing the program we're about to run.
  // """

  sigset_t set;
  if (sigemptyset(&set) != 0) {
    return errno;
  }
  sigset_t oldSetDontCare;
  if (auto err = pthread_sigmask(SIG_SETMASK, &set, &oldSetDontCare)) {
    return err;
  }
  if (signal(SIGPIPE, SIG_DFL) == SIG_ERR) {
    return errno;
  }
  return 0;
}

}