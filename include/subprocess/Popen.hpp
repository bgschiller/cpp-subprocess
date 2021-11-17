#ifndef SUBPROCESS_POPEN_H_
#define SUBPROCESS_POPEN_H_
#include <stdio.h>

#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ChildState.hpp"
#include "ExitStatus.hpp"
#include "PopenConfig.hpp"
#include "PopenError.hpp"
#include "PrepExec.hpp"
#include "Result.hpp"
#include "vendor/fdstream.hpp"

namespace subprocess {

  class Popen {
   public:
    Popen() = delete;
    // TODO: make a destructor that closes any open std_in, std_err, std_out
    static Result<Popen> create(const std::vector<std::string>& argv, const PopenConfig& cfg);

    /**
     * Wait for the process to finish and return its exit status.
     *
     * If the process has already finished, this will return immediately
     * with the exit status. Calling `wait` after that will return the
     * cached exit status without executing any system calls.
     *
     * # Errors
     *
     * Returns a `PopenError` if a system call fails in an unpredicted way.
     * This should not happen in normal usage.
     */
    Result<ExitStatus> wait();

    /**
     * Check whether the process is still running, without blocking or errors.
     *
     * This checks whether the process is still running and returns nullopt if it is. Otherwise, an
     * exit status is returned. This method is guaranteed not to block
     */
    std::optional<ExitStatus> poll();

    /**
     * Return the exit status of the subprocess, if it is known to have finished.
     *
     * Note that this method won't actually *check* whether the child
     * process has finished, it only returns the previously available
     * information.  To check or wait for the process to finish, call
     * `wait`, `wait_timeout`, or `poll`.
     */
    std::optional<ExitStatus> exit_status() const;

    /**
     * Return the PID of the subprocess, if it is known to be still running.
     *
     * Note that this method won't actually *check* whether the child
     * process is still running, it will only return information last set using
     * one of `create`, `wait`, `wait_timeout`, or `poll`. For a newly created
     * `Popen`, `pid()` always returns a value (not nullopt)
     */
    std::optional<pid_t> pid() const;

    /**
     * Wait for the process to finish, timing out after the specified duration.
     *
     * This function behaves like `wait()`, except that the caller will be blocked
     * for roughly no longer than `dur`. It returns `Ok(None)` if the timeout is known
     * to have elapsed.
     *
     * On unix-like systems (all we currently support), timeout is implemented by calling
     * `waitpid(..., WNOHANG)` in a loop with adaptive sleep intervals between iterations.
     */
    Result<std::optional<ExitStatus>> wait_timeout(std::chrono::milliseconds us);

    ChildState child_state;
    bool detached;

    std::optional<boost::fdostream> std_in {std::nullopt};
    std::optional<boost::fdistream> std_out {std::nullopt};
    std::optional<boost::fdistream> std_err {std::nullopt};

   private:
    std::optional<PopenError> os_start(const std::vector<std::string>& argv, const PopenConfig& cfg);
    // Create the pipes requested by stdin, stdout, and stderr from
    // the PopenConfig used to construct us, and return the file-
    // descriptors to be given to the child process.
    //
    // For Redirection::Pipe, this stores the parent end of the pipe
    // to the appropriate self.std* field, and returns the child end
    // of the pipe.
    Result<std::tuple<int, int, int>> setup_streams(const Redirection&& stin, const Redirection&& stout, const Redirection&& sterr);

    Result<const std::nullopt_t> waitpid(bool block);

    int32_t do_exec(
      PrepExec& just_exec,
      std::tuple<int, int, int> child_ends,
      std::optional<std::string> cwd,
      std::optional<uint32_t> setuid,
      std::optional<uint32_t> setgid,
      bool setpgid
    );
  };
}  // namespace subprocess
#endif
