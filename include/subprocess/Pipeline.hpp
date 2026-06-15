#ifndef SUBPROCESS_PIPELINE_H_
#define SUBPROCESS_PIPELINE_H_

#include <filesystem>
#include <optional>
#include <vector>

#include "subprocess/CaptureData.hpp"
#include "subprocess/Exec.hpp"
#include "subprocess/ExitStatus.hpp"
#include "subprocess/Popen.hpp"
#include "subprocess/PopenError.hpp"
#include "subprocess/Redirection.hpp"
#include "subprocess/Result.hpp"

namespace subprocess {

  /// A chain of two or more [`Exec`] configurations connected by pipes,
  /// modelled on the shell pipe operator (`|`).
  ///
  /// Stdout of process N is connected to stdin of process N+1 via an
  /// anonymous OS pipe.  The first process inherits (or uses the configured)
  /// stdin, and the last process inherits (or uses the configured) stdout.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto result = (subprocess::Exec::cmd("cat") | subprocess::Exec::cmd("grep").arg("foo"))
  ///                   .capture()
  ///                   .or_throw();
  /// ```
  class Pipeline {
    // File-redirection operators need access to deferred_error_ so they can
    // store a file-open failure without changing the return type from Pipeline.
    friend Pipeline operator<(Pipeline lhs, const std::filesystem::path& rhs);
    friend Pipeline operator>(Pipeline lhs, const std::filesystem::path& rhs);
    friend Pipeline operator>>(Pipeline lhs, const std::filesystem::path& rhs);

   public:
    /// Create a pipeline from two `Exec` configurations.
    Pipeline(Exec first, Exec second);

    /// Extend the pipeline with another stage.
    Pipeline& pipe(Exec next);

    /// Configure stdin of the first process in the pipeline.
    Pipeline& set_stdin(Redirection r);
    /// @overload
    Pipeline& set_stdin(NullFile);

    /// Configure stdout of the last process in the pipeline.
    Pipeline& set_stdout(Redirection r);
    /// @overload
    Pipeline& set_stdout(NullFile);

    /// Configure stderr for all processes in the pipeline.
    Pipeline& set_stderr(Redirection r);
    /// @overload
    Pipeline& set_stderr(NullFile);

    /// Returns `true` if a previous file-redirection operator stored a
    /// deferred error (e.g. the file does not exist or is not readable).
    bool has_deferred_error() const { return deferred_error_.has_value(); }

    /// Launch all processes in the pipeline and return their `Popen` handles.
    ///
    /// If a deferred error was stored by a file-redirection operator
    /// (`operator<`, `operator>`, `operator>>`), or if any stage in the
    /// pipeline carries a deferred error, it is returned immediately
    /// without spawning any children.
    ///
    /// Inter-process pipes are created before any child is spawned.  The
    /// write end of each pipe becomes the stdout of process N, and the read
    /// end becomes the stdin of process N+1.  Both ends of every intermediate
    /// pipe are closed in the parent after all children have been spawned.
    Result<std::vector<Popen>> popen();

    /// Launch all processes, wait for all to exit, and return the exit status
    /// of the last process.
    Result<ExitStatus> join();

    /// Launch all processes, collect stdout of the last process and stderr of
    /// all, wait for all to exit, and return the captured data.
    ///
    /// The returned `CaptureData::stdout` contains output from the final
    /// process; `CaptureData::stderr` concatenates stderr from every process
    /// (in spawn order).  The `CaptureData::exit_status` reflects the last
    /// process's exit status.
    Result<CaptureData> capture();

   private:
    std::vector<Exec> stages_;
    std::optional<Redirection> stdin_override_;
    std::optional<Redirection> stdout_override_;
    std::optional<Redirection> stderr_override_;

    /// Deferred error from a file-redirection operator that failed to open a
    /// file.  Checked at the start of popen() so the error is surfaced at
    /// execution time while keeping operator chaining clean.
    std::optional<PopenError> deferred_error_;
  };

}  // namespace subprocess

#endif  // SUBPROCESS_PIPELINE_H_
