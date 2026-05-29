#ifndef SUBPROCESS_PIPELINE_H_
#define SUBPROCESS_PIPELINE_H_

#include <optional>
#include <vector>

#include "subprocess/CaptureData.hpp"
#include "subprocess/Exec.hpp"
#include "subprocess/ExitStatus.hpp"
#include "subprocess/Popen.hpp"
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
   public:
    /// Create a pipeline from two `Exec` configurations.
    Pipeline(Exec first, Exec second);

    /// Extend the pipeline with another stage.
    Pipeline& pipe(Exec next);

    /// Configure stdin of the first process in the pipeline.
    Pipeline& stdin_(Redirection r);
    /// @overload
    Pipeline& stdin_(NullFile);

    /// Configure stdout of the last process in the pipeline.
    Pipeline& stdout_(Redirection r);
    /// @overload
    Pipeline& stdout_(NullFile);

    /// Configure stderr for all processes in the pipeline.
    Pipeline& stderr_(Redirection r);
    /// @overload
    Pipeline& stderr_(NullFile);

    /// Launch all processes in the pipeline and return their `Popen` handles.
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
  };

  /// Create a two-stage pipeline from two `Exec` configurations.
  Pipeline operator|(Exec lhs, Exec rhs);
  /// Extend a pipeline with another `Exec` stage.
  Pipeline operator|(Pipeline lhs, Exec rhs);

}  // namespace subprocess

#endif  // SUBPROCESS_PIPELINE_H_
