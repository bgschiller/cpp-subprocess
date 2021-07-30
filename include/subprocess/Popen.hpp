#ifndef SUBPROCESS_POPEN_H_
#define SUBPROCESS_POPEN_H_
#include <stdio.h>

#include <optional>
#include <utility>

#include "ChildState.hpp"
#include "ExitStatus.hpp"
#include "PopenConfig.hpp"
#include "PopenError.hpp"
namespace subprocess {

  class Popen {
   public:
    Popen() = delete;
    using Result = std::variant<PopenError, Popen>;
    static Result create(std::initializer_list<std::string> argv, PopenConfig cfg);

    /**
     * Check whether the process is still running, without blocking or errors.
     *
     * This checks whether the process is still running and returns nullopt if it is. Otherwise, an
     * exit status is returned. This method is guaranteed not to block
     */
    std::optional<ExitStatus> poll();

    FILE *stdin;
    FILE *stdout;
    FILE *stderr;

    ChildState child_state;
    bool detached;
   private:
    std::optional<PopenError> os_start(std::initializer_list<std::string> argv, PopenConfig cfg);
  };
}  // namespace subprocess
#endif
