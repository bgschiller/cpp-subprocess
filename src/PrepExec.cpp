#include "subprocess/PrepExec.hpp"

#include <string.h>
#include <unistd.h>

using namespace subprocess;

PrepExec::PrepExec(
  const std::string& _cmd,
  const std::vector<std::string>& args,
  const std::optional<std::vector<std::string>>& env
)
: cmd{_cmd}
, argvec{args}
{
  if (env.has_value()) {
    envvec = RaggedCstrArray{*env};
  }

  // Allocate enough room for "<pathdir>/<command>\0", pathdir
  // being the longest component of PATH.
  size_t max_exe_len = cmd.size() + 2; // one for '/', one for '\0'
  if (cmd.find("/") == std::string::npos) {
     // use the parent's PATH to determine what to exec
    const char* searchPathRaw = std::getenv("PATH");
    if (searchPathRaw != nullptr) {
      searchpath = std::string(searchPathRaw);
      size_t pos = 0;
      size_t thisDirSize = 0;
      size_t biggestDirSize = 0;
      while (searchPathRaw[pos] != '\0') {
        if (searchPathRaw[pos] != ':') {
          thisDirSize++;
          biggestDirSize = thisDirSize > biggestDirSize ? thisDirSize : biggestDirSize;
        } else {
          thisDirSize = 0;
        }
        pos++;
      }
      max_exe_len += biggestDirSize;
    }
  }
  prealloc_exe.reserve(max_exe_len);
}

int32_t PrepExec::exec() {
  // Invoked after fork() - no heap allocation allowed.
  if (searchpath.has_value()) {
    int32_t errCode = 0;
    // POSIX requires execvp and execve, but not execvpe (although
    // glibc provides one), so we have to iterate over PATH ourselves
    size_t start = 0;
    size_t end = searchpath->find(":");
    while (start != std::string::npos) { // for each PATH component,
      // 1. build the full path to the executable, storing
      //   the value in prealloc_exe. prealloc_exe is guaranteed
      //   to be as long as (longest PATH component + 1 for slash + exe name + 1 for null terminator)
      //   (see ctor for prealloc_exe.reserve())
      size_t ix = 0;
      while (start < end) { // 1a. the PATH segment
        prealloc_exe[ix++] = searchpath->at(start++);
      } // exit condition: start == end, the position of the next ":" or std::string::npos
      if (start != std::string::npos) {
        start++;
        end = searchpath->find(":", start);
      }
      prealloc_exe[ix++] = '/'; // 1b. a seperating '/'
      for (auto ch : cmd) { // 1c. the executable name
        prealloc_exe[ix++] = ch;
      }
      prealloc_exe[ix] = '\0'; // 1d. a null-terminator

      // 2. try to exec.
      errCode = libc_exec();
      // if exec succeeds it doesn't return. When we reach
      // this point the executable was not found at that path
    }
    // we haven't found the command anywhere on the path, just return
    // the last error
    return errCode;
  }

  size_t ix = 0;
  for (auto ch: cmd) {
    prealloc_exe[ix++] = ch;
  }
  prealloc_exe[ix] = '\0';
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