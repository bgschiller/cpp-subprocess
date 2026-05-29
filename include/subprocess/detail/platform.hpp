/**
 * @file platform.hpp
 * @brief Platform portability shims shared across all translation units.
 *
 * This header is the single place where Unix vs. Windows type and constant
 * differences are resolved.  Every header and source file that needs a
 * platform-specific type (e.g. `pid_type`, `mode_type`) should include this
 * file rather than pulling in `<unistd.h>` or `<windows.h>` directly.
 */
#ifndef SUBPROCESS_DETAIL_PLATFORM_H_
#define SUBPROCESS_DETAIL_PLATFORM_H_

#include <stdint.h>

#ifdef _WIN32

#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <fcntl.h>
#  include <io.h>
#  include <windows.h>

namespace subprocess {

  /// Platform-specific process-identifier type.
  /// On Unix this is \c pid_t; on Windows it is \c DWORD (unsigned 32-bit).
  using pid_type = DWORD;

  /// Platform-specific file-permission mode type.
  /// On Unix this is \c mode_t; on Windows it maps to the CRT \c int mode.
  using mode_type = int;

}  // namespace subprocess

// ---------------------------------------------------------------------------
// POSIX-compatible O_* flag aliases (Windows CRT uses _O_* names)
// ---------------------------------------------------------------------------
#  ifndef O_RDONLY
#    define O_RDONLY _O_RDONLY
#  endif
#  ifndef O_WRONLY
#    define O_WRONLY _O_WRONLY
#  endif
#  ifndef O_RDWR
#    define O_RDWR _O_RDWR
#  endif
#  ifndef O_CREAT
#    define O_CREAT _O_CREAT
#  endif
#  ifndef O_TRUNC
#    define O_TRUNC _O_TRUNC
#  endif
#  ifndef O_APPEND
#    define O_APPEND _O_APPEND
#  endif
#  ifndef O_BINARY
#    define O_BINARY _O_BINARY
#  endif

// ---------------------------------------------------------------------------
// POSIX-compatible S_I* permission-bit aliases (Windows CRT uses _S_* names)
// ---------------------------------------------------------------------------
#  ifndef S_IRUSR
#    define S_IRUSR _S_IREAD
#    define S_IWUSR _S_IWRITE
#    define S_IRGRP 0
#    define S_IWGRP 0
#    define S_IROTH 0
#    define S_IWOTH 0
#  endif
#  ifndef S_IREAD
#    define S_IREAD _S_IREAD
#  endif
#  ifndef S_IWRITE
#    define S_IWRITE _S_IWRITE
#  endif

#else  // !_WIN32

#  include <fcntl.h>
#  include <sys/stat.h>
#  include <sys/types.h>
#  include <unistd.h>

namespace subprocess {

  /// Platform-specific process-identifier type (Unix: \c pid_t).
  using pid_type = pid_t;

  /// Platform-specific file-permission mode type (Unix: \c mode_t).
  using mode_type = mode_t;

}  // namespace subprocess

#endif  // _WIN32

#endif  // SUBPROCESS_DETAIL_PLATFORM_H_
