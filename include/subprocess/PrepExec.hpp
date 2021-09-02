#ifndef SUBPROCESS_PREP_EXEC_H_
#define SUBPROCESS_PREP_EXEC_H_

#include <optional>
#include <stdint.h>
#include <string>
#include <vector>

#include "RaggedCstrArray.hpp"

namespace subprocess {

/**
 * We wish to avoid allowing the child to allocate after fork()
 *
 * This class, upon instantiation, allocates any memory we
 * will need in order to perform the exec.
 * (in the rust version, it's implemented as a function returning
 *  a FnOnce)
 */
class PrepExec {
  std::string cmd;
  RaggedCStrArray argvec;
  std::optional<RaggedCStrArray> envvec;
  std::optional<std::string> searchpath;
  std::vector<uint8_t> prealloc_exe;

  int32_t PrepExec::libc_exec();

 public:
  PrepExec(
    const std::string& cmd,
    const std::vector<std::string>& args,
    const std::optional<std::vector<std::string>>& env
  );

  int32_t exec();
};

}
#endif