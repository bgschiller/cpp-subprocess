#include "subprocess/Popen.hpp"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <thread>
#include <vector>

#include "subprocess/CaptureData.hpp"
#include "subprocess/posix.hpp"

using namespace subprocess;
using namespace std::chrono_literals;

Result<Popen> Popen::create(const std::vector<std::string>& argv, const PopenConfig& cfg) {
  if (argv.size() == 0) {
    return PopenError{ PopenError::LogicError, "argv must not be empty" };
  }
  Popen inst{ ChildState::Preparing(), cfg.detached };
  auto res = inst.os_start(argv, cfg);
  if (res.has_value()) {
    return *res;
  }
  return Result<Popen>{ std::move(inst) };
}

Popen::~Popen() {
  if (!alive_ || detached) {
    return;
  }
  // Close all open pipe ends so that a child blocked on stdin gets EOF
  // and any buffered data is flushed before we wait.
  std_in.reset();
  std_out.reset();
  std_err.reset();
  // Reap the child if it has not already been waited on.
  if (child_state.is_a<ChildState::Running>()) {
    wait();  // errors are silently ignored inside a destructor
  }
}

Popen::Popen(Popen&& other) noexcept
    : child_state{ std::move(other.child_state) }
    , detached{ other.detached }
    , std_in{ std::move(other.std_in) }
    , std_out{ std::move(other.std_out) }
    , std_err{ std::move(other.std_err) }
    , alive_{ other.alive_ } {
  // Mark the donor as dead so its destructor is a no-op.  We deliberately
  // do not touch `other.detached` — it retains its caller-visible meaning.
  other.alive_ = false;
}

Popen& Popen::operator=(Popen&& other) noexcept {
  if (this != &other) {
    child_state = std::move(other.child_state);
    detached = other.detached;
    std_in = std::move(other.std_in);
    std_out = std::move(other.std_out);
    std_err = std::move(other.std_err);
    alive_ = other.alive_;
    other.alive_ = false;
  }
  return *this;
}

// ---------------------------------------------------------------------------
// Common pipe-setup helpers (used by both Unix and Windows paths via CRT fds)
// ---------------------------------------------------------------------------

Result<boost::fdostream> prepare_pipe_to_child(int& child_end) {
  auto pi = pipe();
  if (!pi.ok()) return pi.take_error();
  auto [read, write] = pi.take_value();
  int parent_end = write;
  child_end = read;
  set_inheritable(parent_end, false);
  return std::move(boost::fdostream(parent_end));
}

Result<boost::fdistream> prepare_pipe_from_child(int& child_end) {
  auto pi = pipe();
  if (!pi.ok()) return pi.take_error();
  auto [read, write] = pi.take_value();
  int parent_end = read;
  child_end = write;
  set_inheritable(parent_end, false);
  return std::move(boost::fdistream(parent_end));
}

Result<const std::nullopt_t> prepare_file(int fd, int& child_end) {
  set_inheritable(fd, true);
  child_end = fd;
  return std::nullopt;
}

#ifdef _WIN32

// ---------------------------------------------------------------------------
// Windows-specific helpers
// ---------------------------------------------------------------------------

/// Quote a single argument following the MSVC / Win32 command-line rules.
/// See: https://docs.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments
static std::string quote_windows_arg(const std::string& arg) {
  // Empty argument must be represented as a pair of double quotes.
  if (arg.empty()) return "\"\"";

  // Check whether quoting is necessary.
  bool needs_quote = arg.find_first_of(" \t\n\v\"") != std::string::npos;
  if (!needs_quote) return arg;

  std::string result = "\"";
  for (size_t i = 0; i < arg.size();) {
    size_t num_backslashes = 0;
    while (i < arg.size() && arg[i] == '\\') {
      ++i;
      ++num_backslashes;
    }
    if (i == arg.size()) {
      // Trailing backslashes before the closing quote must be doubled.
      for (size_t j = 0; j < num_backslashes * 2; ++j) result += '\\';
    } else if (arg[i] == '"') {
      // Backslashes immediately preceding a quote must be doubled, then the
      // quote itself must be escaped with an additional backslash.
      for (size_t j = 0; j < num_backslashes * 2 + 1; ++j) result += '\\';
      result += '"';
      ++i;
    } else {
      // Regular backslashes do not need escaping.
      for (size_t j = 0; j < num_backslashes; ++j) result += '\\';
      result += arg[i++];
    }
  }
  result += '"';
  return result;
}

/// Build a flat CreateProcess-compatible command line from an argv vector.
/// The first element is the program name; all elements are quoted as necessary.
static std::string build_windows_cmdline(const std::vector<std::string>& argv) {
  std::string cmdline;
  for (const auto& arg : argv) {
    if (!cmdline.empty()) cmdline += ' ';
    cmdline += quote_windows_arg(arg);
  }
  return cmdline;
}

/// Build a Windows environment block from a list of KEY=VALUE pairs.
/// The block is a sequence of NUL-terminated strings followed by a final NUL.
static std::string build_windows_env_block(const std::vector<EnvVar>& env) {
  std::string block;
  for (const auto& [key, value] : env) {
    block += key;
    block += '=';
    block += value;
    block += '\0';
  }
  block += '\0';
  return block;
}

/// Obtain the inheritable Windows HANDLE that corresponds to a CRT fd,
/// making it inheritable if it wasn't already.
static HANDLE fd_to_inheritable_handle(int fd) {
  HANDLE raw = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
  // Ensure the handle is marked inheritable so CreateProcess can duplicate it.
  SetHandleInformation(raw, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
  return raw;
}

std::optional<PopenError> Popen::os_start(
    const std::vector<std::string>& argv, const PopenConfig& config) {
  // ---------------------------------------------------------------------------
  // 1.  Build STARTUPINFOA.  We always specify all three std handles so that
  //     the child never accidentally inherits our console streams when a pipe
  //     is set up for a different stream.
  // ---------------------------------------------------------------------------
  STARTUPINFOA si{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;

  // Child-side handles that must be closed after CreateProcess.
  // They are CRT fds (from pipe()) that we convert to HANDLEs for STARTUPINFO.
  // We track them as ints so we can _close() them after the child is spawned.
  int child_stdin_fd = -1;
  int child_stdout_fd = -1;
  int child_stderr_fd = -1;

  // ---- stdin ---------------------------------------------------------------
  {
    const Redirection& r = config.stdin_;
    if (r.is_a<Redirection::Pipe>()) {
      auto stream = prepare_pipe_to_child(child_stdin_fd);
      if (!stream.ok()) return stream.take_error();
      std_in = stream.take_value();
      si.hStdInput = fd_to_inheritable_handle(child_stdin_fd);
    } else if (r.is_a<Redirection::FileDescriptor>()) {
      int fd = r.get<Redirection::FileDescriptor>().fd;
      si.hStdInput = fd_to_inheritable_handle(fd);
    } else {
      // Inherit parent's stdin.
      si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    }
  }

  // ---- stdout / stderr (need to detect Merge before assigning) -------------
  bool err_to_out = config.stderr_.is_a<Redirection::Merge>();
  bool out_to_err = config.stdout_.is_a<Redirection::Merge>();

  {
    const Redirection& r = config.stdout_;
    if (r.is_a<Redirection::Pipe>()) {
      auto stream = prepare_pipe_from_child(child_stdout_fd);
      if (!stream.ok()) return stream.take_error();
      std_out = stream.take_value();
      si.hStdOutput = fd_to_inheritable_handle(child_stdout_fd);
    } else if (r.is_a<Redirection::FileDescriptor>()) {
      int fd = r.get<Redirection::FileDescriptor>().fd;
      si.hStdOutput = fd_to_inheritable_handle(fd);
    } else if (!r.is_a<Redirection::Merge>()) {
      si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    }
  }

  {
    const Redirection& r = config.stderr_;
    if (r.is_a<Redirection::Pipe>()) {
      auto stream = prepare_pipe_from_child(child_stderr_fd);
      if (!stream.ok()) return stream.take_error();
      std_err = stream.take_value();
      si.hStdError = fd_to_inheritable_handle(child_stderr_fd);
    } else if (r.is_a<Redirection::FileDescriptor>()) {
      int fd = r.get<Redirection::FileDescriptor>().fd;
      si.hStdError = fd_to_inheritable_handle(fd);
    } else if (!r.is_a<Redirection::Merge>()) {
      si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    }
  }

  // Apply Merge redirections after both streams have been set up.
  if (err_to_out) {
    si.hStdError = si.hStdOutput;
  } else if (out_to_err) {
    si.hStdOutput = si.hStdError;
  }

  // ---------------------------------------------------------------------------
  // 2.  Assemble the command line.
  //     If config.executable is set, it overrides argv[0] as the image path
  //     but argv[0] still appears as the program name in the command line
  //     (matching the Unix execve() semantics we emulate).
  // ---------------------------------------------------------------------------
  std::string cmdline = build_windows_cmdline(argv);

  // ---------------------------------------------------------------------------
  // 3.  Build the optional environment block.
  // ---------------------------------------------------------------------------
  std::string env_block;
  if (config.env.has_value()) {
    env_block = build_windows_env_block(*config.env);
  }

  // ---------------------------------------------------------------------------
  // 4.  Determine the working directory (nullptr == inherit from parent).
  // ---------------------------------------------------------------------------
  std::string cwd_str;
  const char* cwd_ptr = nullptr;
  if (config.cwd.has_value()) {
    cwd_str = config.cwd->string();
    cwd_ptr = cwd_str.c_str();
  }

  // ---------------------------------------------------------------------------
  // 5.  Launch!
  // ---------------------------------------------------------------------------
  PROCESS_INFORMATION pi{};
  const char* application = nullptr;
  std::string app_str;
  if (config.executable.has_value()) {
    app_str = *config.executable;
    application = app_str.c_str();
  }

  BOOL ok = CreateProcessA(
      application,                                          // lpApplicationName
      cmdline.empty() ? nullptr : cmdline.data(),           // lpCommandLine (mutable!)
      nullptr,                                              // lpProcessAttributes
      nullptr,                                              // lpThreadAttributes
      TRUE,                                                 // bInheritHandles
      0,                                                    // dwCreationFlags
      config.env.has_value() ? env_block.data() : nullptr,  // lpEnvironment
      cwd_ptr,                                              // lpCurrentDirectory
      &si,                                                  // lpStartupInfo
      &pi                                                   // lpProcessInformation
  );

  // Close child-side CRT fds now that CreateProcess has duplicated the handles.
  if (child_stdin_fd != -1) _close(child_stdin_fd);
  if (child_stdout_fd != -1) _close(child_stdout_fd);
  if (child_stderr_fd != -1) _close(child_stderr_fd);

  if (!ok) {
    DWORD err = GetLastError();
    char buf[256]{};
    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, err, 0, buf,
        sizeof(buf) - 1, nullptr);
    return PopenError{ PopenError::IoError, std::string("CreateProcess failed: ") + buf };
  }

  // We don't need the thread handle.
  CloseHandle(pi.hThread);

  child_state = ChildState::Running{ pi.dwProcessId, static_cast<void*>(pi.hProcess) };
  return std::nullopt;
}

Result<const std::nullopt_t> Popen::waitpid_impl(bool block) {
  return child_state.match(
      [](const ChildState::Preparing&) -> Result<const std::nullopt_t> {
        panic("child_state == Preparing");
        return std::nullopt;
      },
      [block, this](const ChildState::Running& r) -> Result<const std::nullopt_t> {
        HANDLE h = static_cast<HANDLE>(r.process_handle);
        DWORD timeout_ms = block ? INFINITE : 0;
        DWORD result = WaitForSingleObject(h, timeout_ms);
        if (result == WAIT_TIMEOUT) {
          return std::nullopt;
        }
        if (result == WAIT_FAILED) {
          return PopenError{ PopenError::IoError,
                             std::string("WaitForSingleObject failed: ") +
                                 std::to_string(static_cast<int>(GetLastError())) };
        }
        DWORD exit_code = 0;
        GetExitCodeProcess(h, &exit_code);
        CloseHandle(h);
        this->child_state =
            ChildState::Finished{ ExitStatus::Exited{ static_cast<int32_t>(exit_code) } };
        return std::nullopt;
      },
      [](const ChildState::Finished&) -> Result<const std::nullopt_t> { return std::nullopt; });
}

Result<std::nullopt_t> Popen::send_signal(int signum) {
  if (!child_state.is_a<ChildState::Running>()) {
    return PopenError{ PopenError::LogicError, "send_signal: process is not running" };
  }
  HANDLE h = static_cast<HANDLE>(child_state.get<ChildState::Running>().process_handle);
  // On Windows only forceful termination is possible; map SIGTERM and
  // SIGKILL to TerminateProcess and reject arbitrary signal numbers.
  if (signum == SIGTERM || signum == SIGKILL) {
    if (!TerminateProcess(h, 1)) {
      return PopenError{ PopenError::IoError,
                         std::string("TerminateProcess failed: ") +
                             std::to_string(static_cast<int>(GetLastError())) };
    }
    return std::nullopt;
  }
  return PopenError{ PopenError::LogicError,
                     "send_signal: arbitrary signal numbers are not supported on Windows" };
}

#else  // !_WIN32

// ---------------------------------------------------------------------------
// Unix-specific helpers and implementations
// ---------------------------------------------------------------------------

enum class MergeKind {
  ErrToOut,  // 2>&1
  OutToErr,  // 1>&2
  None,
};

Result<std::tuple<int, int, int>> Popen::setup_streams(
    const Redirection& stin, const Redirection& stout, const Redirection& sterr) {
  int child_stdin = 0;
  int child_stdout = 1;
  int child_stderr = 2;

  MergeKind merge = MergeKind::None;

  {
    Result<const std::nullopt_t> res = stin.match(
        [&, this](const Redirection::Pipe&) -> Result<const std::nullopt_t> {
          auto stream = prepare_pipe_to_child(child_stdin);
          if (!stream.ok()) return stream.take_error();
          this->std_in = stream.take_value();
          return std::nullopt;
        },
        [&](const Redirection::FileDescriptor& file) { return prepare_file(file.fd, child_stdin); },
        [&](const Redirection::Merge&) -> Result<const std::nullopt_t> {
          return PopenError{ PopenError::LogicError, "Redirection::Merge is not valid for stdin" };
        },
        [] { /* inherit fds */
             return std::nullopt;
        });
    if (!res.ok()) return res.take_error();
  }

  {
    Result<const std::nullopt_t> res = stout.match(
        [&, this](const Redirection::Pipe&) -> Result<const std::nullopt_t> {
          auto stream = prepare_pipe_from_child(child_stdout);
          if (!stream.ok()) return stream.take_error();
          this->std_out = stream.take_value();
          return std::nullopt;
        },
        [&](const Redirection::FileDescriptor& file) {
          return prepare_file(file.fd, child_stdout);
        },
        [&](const Redirection::Merge&) {
          merge = MergeKind::OutToErr;
          return std::nullopt;
        },
        [] { /* inherit fds */
             return std::nullopt;
        });
    if (!res.ok()) return res.take_error();
  }

  {
    Result<const std::nullopt_t> res = sterr.match(
        [&, this](const Redirection::Pipe&) -> Result<const std::nullopt_t> {
          auto stream = prepare_pipe_from_child(child_stderr);
          if (!stream.ok()) return stream.take_error();
          this->std_err = stream.take_value();
          return std::nullopt;
        },
        [&](const Redirection::FileDescriptor& file) {
          return prepare_file(file.fd, child_stderr);
        },
        [&](const Redirection::Merge&) {
          merge = MergeKind::ErrToOut;
          return std::nullopt;
        },
        [] { /* inherit fds */
             return std::nullopt;
        });
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

std::optional<PopenError> Popen::os_start(
    const std::vector<std::string>& argv, const PopenConfig& config) {
  auto exec_fail_pipeR = pipe();
  if (!exec_fail_pipeR.ok()) return exec_fail_pipeR.take_error();
  auto exec_fail_pipe = exec_fail_pipeR.take_value();
  set_inheritable(std::get<0>(exec_fail_pipe), false);
  set_inheritable(std::get<1>(exec_fail_pipe), false);
  {
    auto child_endsR = setup_streams(config.stdin_, config.stdout_, config.stderr_);
    if (!child_endsR.ok()) return child_endsR.take_error();
    auto child_ends = child_endsR.take_value();
    std::optional<std::vector<std::string>> childEnv;
    if (config.env.has_value()) {
      childEnv.emplace(std::vector<std::string>(config.env->size()));
      std::transform(
          config.env->begin(), config.env->end(), std::back_inserter(*childEnv),
          [](const EnvVar& ev) { return ev.first + "=" + ev.second; });
    }
    std::string cmd_to_exec = config.executable.value_or(argv[0]);
    PrepExec preparedExec(cmd_to_exec, argv, childEnv);

    pid_t child_pid = ::fork();
    if (child_pid < 0) {
      return PopenError{ PopenError::IoError, std::string("fork(): ") + strerror(errno) };
    } else if (child_pid == 0) {
      // i am the child
      ::close(std::get<0>(exec_fail_pipe));
      int32_t result = do_exec(
          preparedExec, child_ends, config.cwd, config.setuid, config.setgid, config.setpgid);
      // if we are here, it means that exec has failed. Notify
      // the parent and exit.
      ::write(std::get<1>(exec_fail_pipe), &(result), sizeof(result));
      ::close(std::get<1>(exec_fail_pipe));
      std::exit(127);
    } else {
      if (std::get<0>(child_ends) != 0) ::close(std::get<0>(child_ends));
      if (std::get<1>(child_ends) != 1) ::close(std::get<1>(child_ends));
      if (std::get<2>(child_ends) != 2) ::close(std::get<2>(child_ends));
      child_state = ChildState::Running{ child_pid };
    }
  }
  ::close(std::get<1>(exec_fail_pipe));
  int32_t err;
  auto readCnt = ::read(std::get<0>(exec_fail_pipe), &err, sizeof(err));
  ::close(std::get<0>(exec_fail_pipe));
  if (readCnt == 0) {
    // no error written, ok
    return std::nullopt;
  } else if (readCnt == sizeof(err)) {
    return PopenError{ PopenError::IoError,
                       std::string("Following error reported from exec (within child): ") +
                           strerror(err) };
  } else {
    return PopenError{ PopenError::LogicError, "invalid read_count from exec pipe" };
  }
}

int32_t Popen::do_exec(
    PrepExec& just_exec, std::tuple<int, int, int> child_ends,
    std::optional<std::filesystem::path> cwd, std::optional<uint32_t> setuid,
    std::optional<uint32_t> setgid, bool setpgid) {
  if (cwd.has_value()) {
    if (chdir(cwd->c_str()) != 0) {
      return errno;
    }
  }
  if (std::get<0>(child_ends) != 0) {
    if (::dup2(std::get<0>(child_ends), 0) == -1) {
      return errno;
    }
    ::close(std::get<0>(child_ends));
  }
  if (std::get<1>(child_ends) != 1) {
    if (::dup2(std::get<1>(child_ends), 1) == -1) {
      return errno;
    }
    ::close(std::get<1>(child_ends));
  }
  if (std::get<2>(child_ends) != 2) {
    if (::dup2(std::get<2>(child_ends), 2) == -1) {
      return errno;
    }
    ::close(std::get<2>(child_ends));
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

Result<const std::nullopt_t> Popen::waitpid_impl(bool block) {
  return child_state.match(
      [](const ChildState::Preparing&) -> Result<const std::nullopt_t> {
        panic("child_state == Preparing");
        return std::nullopt;
      },
      [block, this](const ChildState::Running& r) -> Result<const std::nullopt_t> {
        int status = 0;
        pid_t pid = ::waitpid(r.pid, &status, block ? 0 : WNOHANG);
        if (pid < 0) {
          if (errno == ECHILD) {
            // Someone else has waited for the child
            // (another thread, a signal handler...).
            // The PID no longer exists and we cannot
            // find its exit status.
            this->child_state = ChildState::Finished{ ExitStatus::Undetermined{} };
            return std::nullopt;
          }
          return PopenError{ PopenError::IoError, std::string("waitpid: ") + strerror(errno) };
        }
        if (pid == r.pid) {
          this->child_state = ChildState::Finished{ decode_exit_status(status) };
        }
        return std::nullopt;
      },
      [](const ChildState::Finished&) -> Result<const std::nullopt_t> { return std::nullopt; });
}

Result<std::nullopt_t> Popen::send_signal(int signum) {
  if (!child_state.is_a<ChildState::Running>()) {
    return PopenError{ PopenError::LogicError, "send_signal: process is not running" };
  }
  pid_t p = child_state.get<ChildState::Running>().pid;
  if (::kill(p, signum) != 0) {
    return PopenError{ PopenError::IoError, std::string("kill: ") + strerror(errno) };
  }
  return std::nullopt;
}

#endif  // _WIN32

// ---------------------------------------------------------------------------
// Platform-neutral methods
// ---------------------------------------------------------------------------

Result<std::nullopt_t> Popen::terminate() { return send_signal(SIGTERM); }

Result<std::nullopt_t> Popen::kill() { return send_signal(SIGKILL); }

std::optional<ExitStatus> Popen::exit_status() const {
  if (child_state.is_a<ChildState::Finished>()) {
    auto finished = child_state.get<ChildState::Finished>();
    return std::make_optional<ExitStatus>(std::move(finished.exit_status));
  }
  return std::nullopt;
}

std::optional<pid_type> Popen::pid() const {
  if (child_state.is_a<ChildState::Running>()) {
    return child_state.get<ChildState::Running>().pid;
  }
  return std::nullopt;
}

Result<ExitStatus> Popen::wait() {
  while (child_state.is_a<ChildState::Running>()) {
    auto res = waitpid_impl(true);
    if (!res.ok()) return res.take_error();
  }
  return *exit_status();
}

Result<std::optional<ExitStatus>> Popen::wait_timeout(std::chrono::milliseconds us) {
  if (child_state.is_a<ChildState::Finished>()) {
    return std::make_optional(child_state.get<ChildState::Finished>().exit_status);
  }

  auto deadline = std::chrono::system_clock::now() + us;
  // double delay at every iteration, maxing at 100ms
  auto delay = 1ms;

  while (true) {
    auto success = this->waitpid_impl(false);
    if (!success.ok()) return success.take_error();

    if (child_state.is_a<ChildState::Finished>()) {
      return std::make_optional(child_state.get<ChildState::Finished>().exit_status);
    }

    auto now = std::chrono::system_clock::now();
    if (now >= deadline) return std::nullopt;

    auto remaining = deadline - now;
    std::this_thread::sleep_for(std::min<std::chrono::nanoseconds>({ delay, remaining }));
    delay = std::min<std::chrono::milliseconds>({ delay * 2, 100ms });
  }
}

std::optional<ExitStatus> Popen::poll() {
  auto res = wait_timeout(0ms);
  if (!res.ok()) return std::nullopt;
  return res.take_value();
}

Result<CaptureData> Popen::communicate_bytes(std::optional<std::vector<uint8_t>> input) {
  if (input.has_value() && !std_in.has_value()) {
    return PopenError{ PopenError::LogicError,
                       "communicate: input data provided but stdin is not a pipe" };
  }

  // Move the pipe streams out of the member variables so that each thread
  // has exclusive, non-racy ownership of its stream.  Explicitly reset the
  // members afterwards to ensure they are empty regardless of move semantics.
  auto local_in = std::move(std_in);
  std_in.reset();
  auto local_out = std::move(std_out);
  std_out.reset();
  auto local_err = std::move(std_err);
  std_err.reset();

  std::string stdout_data;
  std::string stderr_data;

  std::vector<std::thread> threads;
  threads.reserve(3);

  // Stdin writer thread — writes all input and then closes the pipe so that
  // the child receives EOF.
#ifndef _WIN32
  // On Unix: block SIGPIPE for this thread so that a broken pipe (child
  // exited early) causes write() to fail with EPIPE rather than killing
  // the process.
#endif
  if (local_in.has_value()) {
    threads.emplace_back([&local_in, &input]() {
#ifndef _WIN32
      sigset_t set;
      sigemptyset(&set);
      sigaddset(&set, SIGPIPE);
      pthread_sigmask(SIG_BLOCK, &set, nullptr);
#endif
      if (input.has_value()) {
        const auto& data = *input;
        local_in->write(
            reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
      }
      // Destroying the optional closes the underlying fd, sending EOF.
      local_in.reset();
    });
  }

  // Stdout reader thread.
  if (local_out.has_value()) {
    threads.emplace_back([&local_out, &stdout_data]() { stdout_data = local_out->slurp(); });
  }

  // Stderr reader thread.
  if (local_err.has_value()) {
    threads.emplace_back([&local_err, &stderr_data]() { stderr_data = local_err->slurp(); });
  }

  for (auto& t : threads) {
    t.join();
  }

  auto exit_res = wait();
  if (!exit_res.ok()) return exit_res.take_error();

  return CaptureData{ std::move(stdout_data), std::move(stderr_data), exit_res.take_value() };
}

Result<CaptureData> Popen::communicate(std::optional<std::string> input) {
  std::optional<std::vector<uint8_t>> byte_input;
  if (input.has_value()) {
    byte_input.emplace(input->begin(), input->end());
  }
  return communicate_bytes(std::move(byte_input));
}
