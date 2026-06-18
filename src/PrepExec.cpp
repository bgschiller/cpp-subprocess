#ifndef _WIN32
// PrepExec is the Unix fork/exec helper — it is not used on Windows.

#include "subprocess/detail/PrepExec.hpp"

#include <string.h>
#include <unistd.h>

using namespace subprocess;

PrepExec::PrepExec(
    const std::string& _cmd, const std::vector<std::string>& args,
    const std::optional<std::vector<std::string>>& env)
    : cmd{ _cmd }
    , argvec{ args } {
  if (env.has_value()) {
    envvec = RaggedCstrArray{ *env };
  }

  // Allocate enough room for "<pathdir>/<command>\0", pathdir
  // being the longest component of PATH.
  size_t max_exe_len = cmd.size() + 2;  // one for '/', one for '\0'
  if (strchr(cmd.data(), '/') == nullptr) {
    // Use the parent's PATH to determine what to exec.
    searchpath_cstr = std::getenv("PATH");
    if (searchpath_cstr != nullptr) {
      // Walk PATH to find the longest component for buffer sizing.
      size_t biggestDirSize = 0;
      size_t thisDirSize = 0;
      for (const char* p = searchpath_cstr; *p != '\0'; p++) {
        if (*p != ':') {
          thisDirSize++;
          if (thisDirSize > biggestDirSize) biggestDirSize = thisDirSize;
        } else {
          thisDirSize = 0;
        }
      }
      max_exe_len += biggestDirSize;
    }
  }
  // Resize (not just reserve) so that operator[] / data() is valid
  // and no reallocation can happen in exec().
  prealloc_exe.resize(max_exe_len);
}

int32_t PrepExec::exec() {
  // Invoked after fork() — no heap allocation, no C++ exceptions allowed.
  // All operations below use only noexcept / pure-C primitives.
  const char* cmd_data = cmd.data();
  size_t cmd_len = cmd.size();

  if (searchpath_cstr != nullptr) {
    int32_t errCode = 0;
    // POSIX requires execvp and execve, but not execvpe (although
    // glibc provides one), so we have to iterate over PATH ourselves.
    const char* p = searchpath_cstr;
    while (*p != '\0') {
      // 1. Build the full path to the executable in prealloc_exe.
      //    prealloc_exe is guaranteed to be large enough (see ctor sizing).
      const char* end = p;
      while (*end != '\0' && *end != ':') end++;

      size_t seg_len = static_cast<size_t>(end - p);
      char* buf = prealloc_exe.data();
      memcpy(buf, p, seg_len);                       // the PATH segment
      buf[seg_len] = '/';                            // separator
      memcpy(buf + seg_len + 1, cmd_data, cmd_len);  // command name
      buf[seg_len + 1 + cmd_len] = '\0';             // null terminator

      // 2. Try to exec.
      errCode = libc_exec();
      // If exec succeeds it doesn't return. When we reach this point
      // the executable was not found at this path.

      // 3. Advance to the next PATH segment, or exit loop.
      if (*end == '\0') break;
      p = end + 1;
    }
    // We haven't found the command anywhere on PATH; return the last error.
    return errCode;
  }

  // Command contains a slash — try it directly without PATH search.
  memcpy(prealloc_exe.data(), cmd_data, cmd_len);
  prealloc_exe.data()[cmd_len] = '\0';
  return libc_exec();
}

int32_t PrepExec::libc_exec() {
  if (envvec.has_value()) {
    ::execve(prealloc_exe.data(), argvec.asCharStar(), envvec->asCharStar());
  } else {
    ::execv(prealloc_exe.data(), argvec.asCharStar());
  }
  return errno;
}
#endif  // !_WIN32
