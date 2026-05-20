#ifndef SUBPROCESS_POSIX_H_
#define SUBPROCESS_POSIX_H_

#include <fcntl.h>

#include <iostream>
#include <tuple>

#include "ExitStatus.hpp"
#include "Result.hpp"

namespace subprocess {

  Result<std::tuple<int, int>> pipe();

  void set_inheritable(int fd, bool heritable);

  ExitStatus decode_exit_status(int status);

  void panic(std::string msg);

  int32_t reset_sigpipe();

}  // namespace subprocess
#endif