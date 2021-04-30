#ifndef SUBPROCESS_POPEN_H_
#define SUBPROCESS_POPEN_H_
#include <utility>

#include "ExitStatus.hpp"
#include "PopenConfig.hpp"
namespace subprocess {

  class Popen {
   public:
    Popen() = delete;
    Popen(std::initializer_list<std::string> argv, PopenConfig cfg);

    /**
     * Check whether the process is still running, without blocking or errors.
     *
     * This checks whether the process is still running and returns nullopt if it is. Otherwise, an
     * exit status is returned. This method is guaranteed not to block
     */
    std::optional<ExitStatus> poll();
  };
}  // namespace subprocess
#endif
