#ifndef SUBPROCESS_POPEN_H_
#define SUBPROCESS_POPEN_H_
#include <signal.h>
#include <stdio.h>

#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "CaptureData.hpp"
#include "ChildState.hpp"
#include "ExitStatus.hpp"
#include "PopenConfig.hpp"
#include "PopenError.hpp"
#include "Result.hpp"
#include "detail/platform.hpp"
#include "vendor/fdstream.hpp"

#ifndef _WIN32
#include "PrepExec.hpp"
#endif

namespace subprocess {

  class Popen {
   public:
    Popen() = delete;

    /**
     * Destroy the `Popen` handle.
     *
     * Closes any open pipe ends.  If the process is still running and
     * `detached` is `false`, the destructor applies the
     * `destructor_policy` configured at construction time:
     *
     * - `Wait`:             waits indefinitely for the child to exit.
     * - `Terminate`:        sends SIGTERM then waits.
     * - `TerminateAndKill`: sends SIGTERM, waits up to
     *                        `kill_grace_period`, then sends SIGKILL if
     *                        the child has not exited.
     *
     * Errors from signal delivery and wait are silently swallowed
     * (destructor context cannot throw).
     */
    ~Popen();
    Popen(Popen&& other) noexcept;
    Popen& operator=(Popen&& other) noexcept;
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
     * Send an arbitrary signal to the process.
     *
     * Uses `::kill(pid, signum)` under the hood.  The caller is still
     * responsible for reaping the child with `wait()` after the signal is
     * delivered.
     *
     * # Errors
     *
     * Returns a `PopenError::LogicError` if the process is not in the
     * `Running` state (i.e. it has already finished or was never started).
     * Returns a `PopenError::IoError` if the underlying `kill(2)` syscall
     * fails.
     */
    Result<std::nullopt_t> send_signal(int signum);

    /**
     * Send SIGTERM to the process.
     *
     * Thin wrapper around `send_signal(SIGTERM)`.  The process is given a
     * chance to clean up; call `wait()` afterwards to reap it.
     *
     * # Errors
     *
     * See `send_signal()` for error conditions.
     */
    Result<std::nullopt_t> terminate();

    /**
     * Send SIGKILL to the process.
     *
     * Thin wrapper around `send_signal(SIGKILL)`.  The kernel terminates
     * the process immediately; call `wait()` afterwards to reap it.
     *
     * # Errors
     *
     * See `send_signal()` for error conditions.
     */
    Result<std::nullopt_t> kill();

    /**
     * Send optional input to the child's stdin, collect all stdout and stderr,
     * wait for the process to exit, and return the captured output.
     *
     * Deadlock is avoided by multiplexing the three streams across separate
     * threads: one thread writes to stdin while others drain stdout and stderr
     * concurrently.
     *
     * - If `input` has a value, `std_in` must have been set to
     *   `Redirection::Pipe`; otherwise a `LogicError` is returned.
     * - Stdout/stderr are collected only when the corresponding stream was
     *   configured as `Redirection::Pipe`; otherwise the field in
     *   `CaptureData` is empty.
     * - After all I/O completes, `wait()` is called and the exit status is
     *   stored in `CaptureData::exit_status`.
     *
     * # Errors
     *
     * Returns a `PopenError::LogicError` if `input` is provided but stdin is
     * not a pipe.  Propagates any error returned by `wait()`.
     */
    Result<CaptureData> communicate(std::optional<std::string> input = std::nullopt);

    /**
     * Byte-oriented variant of `communicate()`.
     *
     * Identical to `communicate()` but accepts and returns raw bytes rather
     * than UTF-8 strings, making it suitable for binary data.
     */
    Result<CaptureData> communicate_bytes(std::optional<std::vector<uint8_t>> input = std::nullopt);

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
    std::optional<pid_type> pid() const;

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

    std::optional<boost::fdostream> std_in{ std::nullopt };
    std::optional<boost::fdistream> std_out{ std::nullopt };
    std::optional<boost::fdistream> std_err{ std::nullopt };

   private:
    // True for a fully constructed, non-moved-from instance.  The move
    // constructor clears this on the donor so its destructor is a no-op,
    // keeping the meaning of `detached` strictly caller-visible.
    bool alive_{ true };

    DestructorPolicy destructor_policy_{ DestructorPolicy::Wait };
    std::chrono::milliseconds kill_grace_period_{ 0 };

    Popen(ChildState cs, bool det, DestructorPolicy dp, std::chrono::milliseconds kgp)
        : child_state{ std::move(cs) }
        , detached{ det }
        , destructor_policy_{ dp }
        , kill_grace_period_{ kgp } { }

    std::optional<PopenError> os_start(
        const std::vector<std::string>& argv, const PopenConfig& cfg);
    Result<const std::nullopt_t> waitpid_impl(bool block);

#ifndef _WIN32
    // Create the pipes requested by stdin, stdout, and stderr from
    // the PopenConfig used to construct us, and return the file-
    // descriptors to be given to the child process.
    //
    // For Redirection::Pipe, this stores the parent end of the pipe
    // to the appropriate self.std* field, and returns the child end
    // of the pipe.
    Result<std::tuple<int, int, int>> setup_streams(
        const Redirection& stin, const Redirection& stout, const Redirection& sterr);

    int32_t do_exec(
        PrepExec& just_exec, std::tuple<int, int, int> child_ends,
        std::optional<std::filesystem::path> cwd, std::optional<uint32_t> setuid,
        std::optional<uint32_t> setgid, bool setpgid);
#endif  // !_WIN32
  };
}  // namespace subprocess
#endif
