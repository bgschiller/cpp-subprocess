#include "subprocess/Popen.hpp"
#include "subprocess/posix.hpp"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace subprocess;

Result<Popen> Popen::create(std::vector<std::string> argv, const PopenConfig& cfg) {
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

  {
    Result<const std::nullopt_t> res = stdin.match(
      [&, this](const Redirection::Pipe&){ return prepare_pipe(true, &(this->std_in), child_stdin); },
      [&](const Redirection::File& file){ return prepare_file(file.fd, child_stdin); },
      [&](const Redirection::Merge&){
        return PopenError{PopenError::LogicError, "Redirection::Merge not valid for stdin"};
      },
      []{ /* inherit fds */ return std::nullopt; }
    );
    if (!res.ok()) return res.take_error();
  }

  {
    Result<const std::nullopt_t> res = stdout.match(
      [&, this](const Redirection::Pipe&){ return prepare_pipe(false, &(this->std_out), child_stdout); },
      [&](const Redirection::File& file){ return prepare_file(file.fd, child_stdout); },
      [&](const Redirection::Merge&) { merge = MergeKind::OutToErr; return std::nullopt; },
      []{ /* inherit fds */ return std::nullopt; }
    );
    if (!res.ok()) return res.take_error();
  }

  {
    Result<const std::nullopt_t> res = stderr.match(
      [&, this](const Redirection::Pipe&){ return prepare_pipe(false, &(this->std_err), child_stderr); },
      [&](const Redirection::File& file){ return prepare_file(file.fd, child_stderr); },
      [&](const Redirection::Merge&) { merge = MergeKind::ErrToOut; return std::nullopt; },
      []{ /* inherit fds */ return std::nullopt; }
    );
    if (!res.ok()) return res.take_error();
  }

  // TODO: make sure we test these. Do we need to dup() to get a second reference to the same file?
  if (merge == MergeKind::ErrToOut) {
    child_stderr = child_stdout;
  } else if (merge == MergeKind::OutToErr) {
    child_stdout = child_stderr;
  }

  return std::make_tuple(child_stdin, child_stdout, child_stderr);
}

std::optional<PopenError> Popen::os_start(const std::vector<std::string>& argv, const PopenConfig& config) {
  auto exec_fail_pipeR = pipe();
  if (!exec_fail_pipeR.ok()) return exec_fail_pipeR.take_error();
  auto exec_fail_pipe = exec_fail_pipeR.take_value();
  set_inheritable(std::get<0>(exec_fail_pipe), false);
  set_inheritable(std::get<1>(exec_fail_pipe), false);
  {
    auto child_endsR = setup_streams(config.stdin, config.stdout, config.stderr);
    if (!child_endsR.ok()) return child_endsR.take_error();
    auto child_ends = child_endsR.take_value();
    std::optional<std::vector<std::string>> childEnv;
    if (config.env.has_value()) {
      childEnv.emplace(std::vector<std::string>(config.env->size()));
      std::transform(
        config.env->begin(), config.env->end(),
        std::back_inserter(*childEnv),
        [](const EnvVar& ev) {
          return ev.first + "=" + ev.second;
        });
    }
    std::string cmd_to_exec = config.executable.value_or(argv[0]);
    PrepExec preparedExec(cmd_to_exec, argv, childEnv);

    pid_t child_pid = ::fork();
    if (child_pid < 0) {
      return PopenError{PopenError::IoError, std::string("fork(): ") + strerror(errno)};
    } else if (child_pid == 0) {
      // i am the child
      ::close(std::get<0>(exec_fail_pipe));
      int32_t result = do_exec(
        preparedExec,
        child_ends,
        config.cwd,
        config.setuid,
        config.setgid,
        config.setpgid
      );
      // if we are here, it means that exec has failed. Notify
      // the parent and exit.
      ::write(std::get<1>(exec_fail_pipe), &(result), sizeof(result));
      std::exit(127);
    } else {
      child_state = ChildState::Running{child_pid};
    }
  }
  ::close(std::get<1>(exec_fail_pipe));
  int32_t err;
  auto readCnt = ::read(std::get<0>(exec_fail_pipe), &err, sizeof(err));
  if (readCnt == 0) {
    // no error written, ok
    return std::nullopt;
  } else if (readCnt == sizeof(err)) {
    return PopenError{PopenError::IoError, std::string("Following error reported from exec (within child): ") + strerror(err)};
  } else {
    return PopenError{PopenError::LogicError, "invalid read_count from exec pipe"};
  }
}

int32_t Popen::do_exec(
  PrepExec& just_exec,
  std::tuple<int, int, int> child_ends,
  std::optional<std::string> cwd,
  std::optional<uint32_t> setuid,
  std::optional<uint32_t> setgid,
  bool setpgid
) {
  if (cwd.has_value()) {
    if (chdir(cwd->c_str()) != 0) {
      return errno;
    }
  }
  if (std::get<0>(child_ends) != 0) {
    if (::dup2(std::get<0>(child_ends), 0) == -1) {
      return errno;
    }
  }
  if (std::get<1>(child_ends) != 1) {
    if (::dup2(std::get<1>(child_ends), 1) == -1) {
      return errno;
    }
  }
  if (std::get<2>(child_ends) != 2) {
    if (::dup2(std::get<2>(child_ends), 2) == -1) {
      return errno;
    }
  }

  if (auto err = reset_sigpipe()) {
    return err;
  }

  if (setuid.has_value()) {
    if (::setuid(*setuid) != 0) {
      return errno;
    }
  }

  if (setgid.has_value()) {
    if (::setgid(*setgid) != 0) {
      return errno;
    }
  }

  if (setpgid) {
    if (::setpgid(0, 0) != 0) {
      return errno;
    }
  }
  return just_exec.exec();
}


std::optional<ExitStatus> Popen::exit_status() const {
  if (child_state.is_a<ChildState::Finished>()) {
    auto finished = child_state.get<ChildState::Finished>();
    return std::make_optional<ExitStatus>(std::move(finished.exit_status));
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
    [](const ChildState::Preparing&) -> Result<const std::nullopt_t> { panic("child_state == Preparing"); return std::nullopt; },
    [block, this](const ChildState::Running& r) -> Result<const std::nullopt_t> {
      int status = 0;
      pid_t pid = ::waitpid(r.pid, &status, block ? 0 : WNOHANG);
      if (pid < 0) {
        if (errno == ECHILD) {
          // Someone else has waited for the child
          // (another thread, a signal handler...).
          // The PID no longer exists and we cannot
          // find its exit status.
          this->child_state = ChildState::Finished{ExitStatus::Undetermined{}};
          return std::nullopt;
        }
        return PopenError{PopenError::IoError, std::string("waitpid: ") + strerror(errno)};
      }
      if (pid == r.pid) {
        this->child_state = ChildState::Finished{ExitStatus::Exited{status}};
      }
      return std::nullopt;
    },
    [](const ChildState::Finished&) -> Result<const std::nullopt_t> { return std::nullopt; }
  );
}

