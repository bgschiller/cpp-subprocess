#ifndef SUBPROCESS_POPEN_CONFIG_H_
#define SUBPROCESS_POPEN_CONFIG_H_
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace subprocess {

  struct PopenConfig {
    using EnvVar = std::pair<std::string, std::string>;
    /**
     * Environment variables to pass to the subprocess
     *
     * If this is nullopt, environment variables are inherited from the calling process.
     * Otherwise, the specified variables are used instead.
     *
     * Duplicates are eliminated, with the value taken from the variable appearing later in the
     * vector.
     */
    std::optional<std::vector<EnvVar>> env;

    /**
     * Initial current working directory of the subprocess.
     *
     * nullopt means inherit the working directory from the parent.
     */
    std::optional<std::string> cwd;
  };

}  // namespace subprocess

#endif
