#ifndef SUBPROCESS_POPEN_CONFIG_H_
#define SUBPROCESS_POPEN_CONFIG_H_
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Redirection.hpp"

namespace subprocess {

  using EnvVar = std::pair<std::string, std::string>;

  struct PopenConfig {
    /// How to configure the executed program's standard input.
    Redirection stdin{ Redirection::None() };
    /// How to configure the executed program's standard output.
    Redirection stdout{ Redirection::None() };
    /// How to configure the executed program's standard error.
    Redirection stderr{ Redirection::None() };
    /// Whether the `Popen` instance is initially detached.
    bool detached{ false };

    /// Executable to run.
    ///
    /// If provided, this executable will be used to run the program
    /// instead of `argv[0]`.  However, `argv[0]` will still be passed
    /// to the subprocess, which will see that as `argv[0]`.  On some
    /// Unix systems, `ps` will show the string passed as `argv[0]`,
    /// even though `executable` is actually running.
    std::optional<std::string> executable{ std::nullopt };

    /**
     * Environment variables to pass to the subprocess
     *
     * If this is nullopt, environment variables are inherited from the calling
     * process. Otherwise, the specified variables are used instead.
     *
     * Duplicates are eliminated, with the value taken from the variable appearing
     * later in the vector.
     */
    std::optional<std::vector<EnvVar>> env{ std::nullopt };

    /**
     * Initial current working directory of the subprocess.
     *
     * nullopt means inherit the working directory from the parent.
     */
    std::optional<std::string> cwd{ std::nullopt };

    /// Set user ID for the subprocess.
    ///
    /// If specified, calls `setuid()` before execing the child process.
    std::optional<int32_t> setuid{ std::nullopt };

    /// Set group ID for the subprocess.
    ///
    /// If specified, calls `setgid()` before execing the child process.
    std::optional<int32_t> setgid{ std::nullopt };

    /// Returns the environment of the current process.
    ///
    /// The returned value is in the format accepted by the `env`
    /// member of `PopenConfig`.
    static std::vector<EnvVar> currentEnv();
  };

}  // namespace subprocess

#endif
