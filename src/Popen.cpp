#include "subprocess/Popen.hpp"

#include "subprocess/posix.hpp"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace subprocess;

Result<Popen> Popen::create(std::initializer_list<std::string> argv, const PopenConfig& cfg) {
  if (argv.size() == 0) {
    return PopenError{PopenError::LogicError, "argv must not be empty"};
  }
  Popen inst{
    nullptr, nullptr, nullptr,
    ChildState::Preparing(),
    cfg.detached
  };
  auto res = inst.os_start(argv, cfg);
  if (res.has_value()) {
    return *res;
  }
  return Result<Popen>{std::move(inst)};
}


Result<const std::nullopt_t> prepare_pipe(bool parent_writes, FILE** parent_ref, int& child_end) {
  auto pi = pipe();
  if (!pi.ok()) return pi.take_error();
  auto [read, write] = pi.take_value();
  int parent_end;
  if (parent_writes) {
    parent_end = write;
    child_end = read;
  } else {
    parent_end = read;
    child_end = write;
  }
  set_inheritable(parent_end, false);
  *parent_ref = fdopen(parent_end, parent_writes ? "w" : "r");
  if (*parent_ref == nullptr) {
    char* errmsg = strerror(errno);
    ::close(read);
    ::close(write);
    return PopenError{PopenError::IoError, std::string("Failed to create FILE* from file descriptor. fdopen(): ") + errmsg};
  }
  return std::nullopt;
}

Result<const std::nullopt_t> prepare_file(int fd, int& child_end) {
  set_inheritable(fd, true);
  child_end = fd;
  return std::nullopt;
}

enum class MergeKind {
  ErrToOut, // 2>&1
  OutToErr, // 1>&2
  None,
};


Result<std::tuple<int, int, int>> Popen::setup_streams(Redirection stdin, Redirection stdout, Redirection stderr) {
  int child_stdin = 0, child_stdout = 1, child_stderr = 2;
  MergeKind merge = MergeKind::None;

  Result<const std::nullopt_t> res;
  res = stdin.match(
    [&, this](const Redirection::Pipe&){ return prepare_pipe(true, &(this->std_in), child_stdin); },
    [&](const Redirection::File& file){ return prepare_file(file.fd, child_stdin); },
    [&](const Redirection::Merge&){
      return PopenError{PopenError::LogicError, "Redirection::Merge not valid for stdin"};
    },
    []{ /* inherit fds */ return std::nullopt; }
  );
  if (!res.ok()) return res.take_error();

  res = stdout.match(
    [&, this](const Redirection::Pipe&){ return prepare_pipe(false, &(this->std_out), child_stdout); },
    [&](const Redirection::File& file){ return prepare_file(file.fd, child_stdout); },
    [&](const Redirection::Merge&) { merge = MergeKind::OutToErr; return std::nullopt; },
    []{ /* inherit fds */ return std::nullopt; }
  );
  if (!res.ok()) return res.take_error();

  res = stderr.match(
    [&, this](const Redirection::Pipe&){ return prepare_pipe(false, &(this->std_err), child_stderr); },
    [&](const Redirection::File& file){ return prepare_file(file.fd, child_stderr); },
    [&](const Redirection::Merge&) { merge = MergeKind::ErrToOut; return std::nullopt; },
    []{ /* inherit fds */ return std::nullopt; }
  );
  if (!res.ok()) return res.take_error();

  // TODO: make sure we test these. Do we need to dup() to get a second reference to the same file?
  if (merge == MergeKind::ErrToOut) {
    child_stderr = child_stdout;
  } else if (merge == MergeKind::OutToErr) {
    child_stdout = child_stderr;
  }

  return std::make_tuple(child_stdin, child_stdout, child_stderr);
}

std::optional<PopenError> Popen::os_start(const std::initializer_list<std::string>& argv, const PopenConfig& config) {
  auto exec_fail_pipeR = pipe();
  if (!exec_fail_pipeR.ok()) return exec_fail_pipe.take_error();
  auto exec_fail_pipe = exec_fail_pipeR.take_value();
  set_inheritable(std::get<0>(exec_fail_pipe), false);
  set_inheritable(std::get<1>(exec_fail_pipe), false);
  {
    auto child_endsR = setup_streams(config.stdin, config.stdout, config.stderr);
    if (!child_endsR.ok()) return child_endsR.take_error();
    auto child_ends = child_endsR.take_value();
  }
}

std::optional<ExitStatus> Popen::exit_status() const {
  if (child_state.is_a<ChildState::Finished>()) {
    return child_state.get<ChildState::Finished>();
  }
  return std::nullopt;
}

std::optional<pid_t> Popen::pid() const {
  if (child_state.is_a<ChildState::Running>()) {
    return child_state.get<ChildState::Running>().pid;
  }
  return std::nullopt;
}


Result<ExitStatus> Popen::wait() {
  while (child_state.is_a<ChildState::Running>()) {
    auto res = waitpid(true);
    if (!res.ok()) return res.take_error();
  }
  return *exit_status();
}



Result<const std::nullopt_t> Popen::waitpid(bool block) {
  return child_state.match(
    [](const ChildState::Preparing&){ panic("child_state == Preparing"); return std::nullopt; },
    [](const ChildState::Running& r){
      int status = 0;
      pid_t pid = ::waitpid(r.pid, &status, block ? 0 : WNOHANG);
      if (pid < 0) {
        if (errno == ECHILD) {
          // Someone else has waited for the child
          // (another thread, a signal handler...).
          // The PID no longer exists and we cannot
          // find its exit status.
          this->child_state = ChildState::Finished{ExitStatus::Undetermined{}};
          return;
        }
        return PopenError{PopenError::IoError, sterror("waitpid: ")};
      }
      if (pid == r.pid) {
        this->child_state = ChildState::Finished{ExitStatus::Exited{status}};
      }
      return std::nullopt;
    },
    [](const ChildState::Finished&){ return std::nullopt; }
  );
}

