#include "subprocess/Popen.hpp"

#include <fcntl.h>

using namespace subprocess;

Popen::Result Popen::create(const std::initializer_list<std::string>& argv, const PopenConfig& cfg) {
  if (argv.size() == 0) {
    return PopenError{PopenError::LogicError, "argv must not be empty"};
  }
  Popen inst{
    nullptr, nullptr, nullptr,
    ChildState::Preparing(),
    cfg.detached
  };
  auto res = inst.os_start();
  if (res.has_value()) {
    return *res;
  }
  return Popen::Result{std::move(inst)};
}

Result<std::tuple<int, int>> make_pipe() {
  int exec_fail_pipe[2];
  if (::pipe(exec_fail_pipe) != 0) {
    return PopenError{PopenError::ErrKind::IoError, std::string("pipe(): ") + std::string(errno) + strerror(errno)};
  }
  return std::make_tuple(exec_fail_pipe[0], exec_fail_pipe[1]);
}

Result<void_t> prepare_pipe(bool parent_writes, FILE** parent_ref, FILE** child_ref) {
  auto pi = make_pipe();
  if (!pi.ok()) return pi.take_error();
  auto [read, write] = pi.take_value();
  int parent_end, child_end;
  if (parent_writes) {
    parent_end = write;
    child_end = read;
  } else {
    parent_end = read;
    child_end = write;
  }
  fcntl(parent_end, F_SETFD, FD_CLOEXEC);
  *parent_ref = fdopen(parent_end, parent_writes ? "w" : "r");
  if (*parent_ref == nullptr) {
    char* errmsg = strerror(errno);
    ::close(read);
    ::close(write);
    return PopenError{PopenError::IoError, std::string("Failed to create FILE* from file descriptor. fdopen(): ") + errmsg};
  }
  *child_ref = fdopen(child_end, parent_writes ? "r" : "w");
  if (*child_ref == nullptr) {
    char* errmsg = strerror(errno);
    ::close(read);
    ::close(write);
    return PopenError{PopenError::IoError, std::string("Failed to create FILE* from file descriptor. fdopen(): ") + errmsg};
  }
  return std::nullopt;
}

std::optional<PopenError> Popen::os_start(const std::initializer_list<std::string>& argv, const PopenConfig& config) {
  {

  }
}

