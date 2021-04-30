#ifndef SUBPROCESS_POPEN_H_
#define SUBPROCESS_POPEN_H_
#include <stdint.h>

#include <string>

#include "subprocess/ExitStatus.hpp"

namespace subprocess {
  struct CaptureData {
    const std::string stdout;
    const std::string stderr;
    const ExitStatus exit_status;

    bool success() const { return exit_status.success(); }
  };
}  // namespace subprocess
#endif
