# Add `Popen` signal / termination methods

## Problem

There is no way to signal or forcibly stop a running child process.
The Rust subprocess crate provides:

- `terminate()` — sends SIGTERM on Unix
- `kill()` — sends SIGKILL on Unix
- `send_signal(signal)` — sends an arbitrary signal on Unix

None of these exist in the C++ implementation.

## Proposed API

```cpp
// include/subprocess/Popen.hpp

/// Send SIGTERM to the process.
/// Returns an error if the process is not running or the syscall fails.
Result<std::nullopt_t> terminate();

/// Send SIGKILL to the process.
/// Returns an error if the process is not running or the syscall fails.
Result<std::nullopt_t> kill();

/// Send an arbitrary signal to the process.
/// Returns an error if the process is not running or the syscall fails.
Result<std::nullopt_t> send_signal(int signum);
```

`terminate()` and `kill()` should be thin wrappers around `send_signal`.

## Implementation notes

- Use `::kill(pid, signum)` (from `<signal.h>`).
- Check that `child_state` is `Running` before calling; return a
  `PopenError::LogicError` if the process has already finished.
- After `terminate()` or `kill()`, the caller still needs to call `wait()`
  to reap the child — these methods only send the signal.

## Affected files

- `include/subprocess/Popen.hpp`
- `src/Popen.cpp`

## Notes

- Depends on ticket **01** (correct exit status decoding) so that a
  `wait()` after `kill()` correctly reports `ExitStatus::Signaled`.
- Windows equivalent would be `TerminateProcess()`; leave a
  `#ifdef _WIN32` stub for now.
