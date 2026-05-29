#include "subprocess/Pipeline.hpp"

#ifdef _WIN32
#define SUBPROCESS_DEVNULL "NUL"
#else
#define SUBPROCESS_DEVNULL "/dev/null"
#endif

#include <stdexcept>

#include "subprocess/PopenError.hpp"
#include "subprocess/posix.hpp"

namespace subprocess {

  Pipeline::Pipeline(Exec first, Exec second) {
    stages_.push_back(std::move(first));
    stages_.push_back(std::move(second));
  }

  Pipeline& Pipeline::pipe(Exec next) {
    stages_.push_back(std::move(next));
    return *this;
  }

  Pipeline& Pipeline::stdin_(Redirection r) {
    stdin_override_ = std::move(r);
    return *this;
  }

  Pipeline& Pipeline::stdin_(NullFile) {
    stdin_override_ = Redirection::Read(SUBPROCESS_DEVNULL).or_throw();
    return *this;
  }

  Pipeline& Pipeline::stdout_(Redirection r) {
    stdout_override_ = std::move(r);
    return *this;
  }

  Pipeline& Pipeline::stdout_(NullFile) {
    stdout_override_ = Redirection::Write(SUBPROCESS_DEVNULL).or_throw();
    return *this;
  }

  Pipeline& Pipeline::stderr_(Redirection r) {
    stderr_override_ = std::move(r);
    return *this;
  }

  Pipeline& Pipeline::stderr_(NullFile) {
    stderr_override_ = Redirection::Write(SUBPROCESS_DEVNULL).or_throw();
    return *this;
  }

  Result<std::vector<Popen>> Pipeline::popen() {
    const std::size_t n = stages_.size();
    if (n < 2) {
      return PopenError{ PopenError::LogicError, "Pipeline must have at least two stages" };
    }

    // Create n-1 inter-process pipes: pipes[i] connects stage i (write end)
    // to stage i+1 (read end).
    // Each element is {read_fd, write_fd}.
    std::vector<std::pair<int, int>> pipes;
    pipes.reserve(n - 1);
    for (std::size_t i = 0; i < n - 1; ++i) {
      auto pipe_result = subprocess::pipe();
      if (!pipe_result.ok()) {
        // Close any already-created pipes before returning.
        for (auto& [r, w] : pipes) {
          ::close(r);
          ::close(w);
        }
        return pipe_result.take_error();
      }
      auto [read_fd, write_fd] = pipe_result.take_value();
      pipes.push_back({ read_fd, write_fd });
    }

    std::vector<Popen> children;
    children.reserve(n);

    for (std::size_t i = 0; i < n; ++i) {
      Exec& stage = stages_[i];

      // --- stdin ---
      if (i == 0) {
        if (stdin_override_) {
          stage.stdin_(std::move(*stdin_override_));
          stdin_override_ = std::nullopt;
        }
      } else {
        // Read end of the previous inter-process pipe.
        stage.stdin_(Redirection::FileDescriptor(pipes[i - 1].first));
      }

      // --- stdout ---
      if (i == n - 1) {
        if (stdout_override_) {
          stage.stdout_(std::move(*stdout_override_));
          stdout_override_ = std::nullopt;
        }
      } else {
        // Write end of the next inter-process pipe.
        stage.stdout_(Redirection::FileDescriptor(pipes[i].second));
      }

      // --- stderr ---
      if (stderr_override_) {
        // We need to duplicate the override for each stage (move it into the
        // last one, copy for earlier ones by duplicating the fd).
        if (i == n - 1) {
          stage.stderr_(std::move(*stderr_override_));
          stderr_override_ = std::nullopt;
        } else {
          // Duplicate the fd so each stage gets its own copy.
          if (stderr_override_->is_a<Redirection::FileDescriptor>()) {
            int orig_fd = stderr_override_->get<Redirection::FileDescriptor>().fd;
            int dup_fd = ::dup(orig_fd);
            if (dup_fd == -1) {
              return PopenError{ PopenError::IoError, "dup() failed for stderr redirection" };
            }
            stage.stderr_(Redirection::FileDescriptor(dup_fd));
          } else if (stderr_override_->is_a<Redirection::Pipe>()) {
            stage.stderr_(Redirection::Pipe{});
          } else if (stderr_override_->is_a<Redirection::Merge>()) {
            stage.stderr_(Redirection::Merge{});
          }
          // None: do nothing
        }
      }

      auto pop_result = stage.popen();
      if (!pop_result.ok()) {
        // Close remaining pipe fds before returning.
        for (std::size_t j = i; j < n - 1; ++j) {
          ::close(pipes[j].first);
          ::close(pipes[j].second);
        }
        return pop_result.take_error();
      }
      children.push_back(pop_result.take_value());
    }

    // Close all intermediate pipe fds in the parent now that all children
    // have been spawned — the children hold the relevant ends open.
    for (auto& [r, w] : pipes) {
      ::close(r);
      ::close(w);
    }

    return children;
  }

  Result<ExitStatus> Pipeline::join() {
    auto procs_result = popen();
    if (!procs_result.ok()) return procs_result.take_error();
    auto procs = procs_result.take_value();

    ExitStatus last_status{ ExitStatus::Undetermined{} };
    for (auto& proc : procs) {
      auto wait_result = proc.wait();
      if (!wait_result.ok()) return wait_result.take_error();
      last_status = wait_result.take_value();
    }
    return last_status;
  }

  Result<CaptureData> Pipeline::capture() {
    const std::size_t n = stages_.size();

    // We want to capture stdout of the last process and stderr of all processes.
    // Configure stdout of the last stage as a pipe (if not already set).
    if (!stdout_override_) {
      stdout_override_ = Redirection::Pipe();
    }

    // We need individual stderr pipes for each stage.  We'll collect them
    // by spawning and then reading from each.  Rather than fight with the
    // stderr_override_ mechanism (which broadcasts a single fd), we apply
    // Pipe() to each stage's stderr directly before calling popen().
    for (Exec& stage : stages_) {
      stage.stderr_(Redirection::Pipe());
    }

    auto procs_result = popen();
    if (!procs_result.ok()) return procs_result.take_error();
    auto procs = procs_result.take_value();

    // Drain stdout from the last process and stderr from all processes
    // concurrently to avoid deadlocks.  We use communicate() on the last
    // process for stdout + its stderr, and slurp() the other stderr pipes.

    // Collect stderr from all-but-last processes first (they are expected to
    // produce modest stderr output; for large stderr the user should redirect
    // to a file instead).
    std::string combined_stderr;
    for (std::size_t i = 0; i < n - 1; ++i) {
      if (procs[i].std_err.has_value()) {
        combined_stderr += procs[i].std_err->slurp();
      }
      procs[i].wait();
    }

    // communicate() on the last process handles stdout + its stderr.
    auto comm_result = procs[n - 1].communicate();
    if (!comm_result.ok()) return comm_result.take_error();
    auto comm = comm_result.take_value();

    combined_stderr += comm.err;

    return CaptureData{ std::move(comm.out), std::move(combined_stderr),
                        std::move(comm.exit_status) };
  }

  Pipeline operator|(Exec lhs, Exec rhs) { return Pipeline{ std::move(lhs), std::move(rhs) }; }

  Pipeline operator|(Pipeline lhs, Exec rhs) {
    lhs.pipe(std::move(rhs));
    return lhs;
  }

}  // namespace subprocess
