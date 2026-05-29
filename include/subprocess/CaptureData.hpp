#ifndef SUBPROCESS_CAPTURE_DATA_H_
#define SUBPROCESS_CAPTURE_DATA_H_

#include <string>

#include "subprocess/ExitStatus.hpp"

namespace subprocess {

  /// Data returned by `Popen::communicate()` and `Popen::communicate_bytes()`.
  ///
  /// Fields are populated after all I/O is complete and the child process has
  /// been waited on.  A field is empty if the corresponding stream was not
  /// redirected to a `Redirection::Pipe`.
  struct CaptureData {
    /// All bytes written by the child to stdout (empty if stdout was not piped).
    std::string out;
    /// All bytes written by the child to stderr (empty if stderr was not piped).
    std::string err;
    /// Exit status of the child process after `communicate()` completes.
    ExitStatus exit_status;

    /// Constructs a `CaptureData` with the given stdout, stderr, and exit status.
    CaptureData(std::string stdout_data, std::string stderr_data, ExitStatus status)
        : out{ std::move(stdout_data) }
        , err{ std::move(stderr_data) }
        , exit_status{ std::move(status) } { }

    /// Returns `true` if the child exited with status 0.
    bool success() const { return exit_status.success(); }
  };

}  // namespace subprocess

#endif  // SUBPROCESS_CAPTURE_DATA_H_
