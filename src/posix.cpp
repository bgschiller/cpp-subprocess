#include "subprocess/detail/posix.hpp"

#include <string.h>

#ifdef _WIN32
#include <errno.h>
#include <io.h>
#else
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#endif

#include "subprocess/PopenError.hpp"

namespace subprocess {

  Result<std::tuple<int, int>> pipe() {
#ifdef _WIN32
    // CreatePipe gives us inheritable HANDLEs; convert them to CRT fds so that
    // fdstream can work with them uniformly on all platforms.
    HANDLE read_h = INVALID_HANDLE_VALUE;
    HANDLE write_h = INVALID_HANDLE_VALUE;
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    if (!CreatePipe(&read_h, &write_h, &sa, 0)) {
      return PopenError{ PopenError::ErrKind::IoError,
                         std::string("CreatePipe failed: ") +
                             std::to_string(static_cast<int>(GetLastError())) };
    }
    int read_fd = _open_osfhandle(reinterpret_cast<intptr_t>(read_h), _O_RDONLY | _O_BINARY);
    int write_fd = _open_osfhandle(reinterpret_cast<intptr_t>(write_h), _O_WRONLY | _O_BINARY);
    if (read_fd == -1 || write_fd == -1) {
      if (read_fd != -1) _close(read_fd);
      if (write_fd != -1) _close(write_fd);
      CloseHandle(read_h);
      CloseHandle(write_h);
      return PopenError{ PopenError::ErrKind::IoError, "_open_osfhandle failed" };
    }
    return std::make_tuple(read_fd, write_fd);
#else
    int pipe_fds[2];
    if (::pipe(pipe_fds) != 0) {
      return PopenError{ PopenError::ErrKind::IoError, std::string("pipe(): ") +
                                                           std::to_string(errno) +
                                                           std::string(" ") + strerror(errno) };
    }
    return std::make_tuple(pipe_fds[0], pipe_fds[1]);
#endif
  }

  void set_inheritable(int fd, bool heritable) {
#ifdef _WIN32
    HANDLE h = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
    if (h != INVALID_HANDLE_VALUE) {
      SetHandleInformation(h, HANDLE_FLAG_INHERIT, heritable ? HANDLE_FLAG_INHERIT : 0);
    }
#else
    int curr = fcntl(fd, F_GETFD);
    fcntl(fd, F_SETFD, heritable ? (curr & ~FD_CLOEXEC) : (curr | FD_CLOEXEC));
#endif
  }

  int close_fd(int fd) {
#ifdef _WIN32
    return _close(fd);
#else
    return ::close(fd);
#endif
  }

  int dup_fd(int fd) {
#ifdef _WIN32
    return _dup(fd);
#else
    return ::dup(fd);
#endif
  }

  std::string error_string(int errnum) {
#ifdef _WIN32
    char buf[256] = { 0 };
    if (strerror_s(buf, sizeof(buf), errnum) == 0) {
      return std::string(buf);
    }
    return std::string("errno ") + std::to_string(errnum);
#else
    return std::string(strerror(errnum));
#endif
  }

  void panic(std::string msg) {
    std::cerr << msg << std::endl;
    std::exit(1);
  }

#ifndef _WIN32
  ExitStatus decode_exit_status(int status) {
    if (WIFEXITED(status)) {
      return ExitStatus::Exited{ WEXITSTATUS(status) };
    } else if (WIFSIGNALED(status)) {
      return ExitStatus::Signaled{ WTERMSIG(status) };
    } else {
      return ExitStatus::Other{ status };
    }
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
#endif  // !_WIN32

}  // namespace subprocess
