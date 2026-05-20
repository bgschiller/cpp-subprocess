# Windows support

## Problem

The entire implementation is Unix-only. Every system call used
(`fork`, `exec`, `pipe`, `waitpid`, `kill`, `dup2`, `chdir`, `setuid`,
`setgid`, `setpgid`) is a POSIX API with no Windows equivalent.
The CI matrix in the README template references Windows but the library
cannot build there.

## Scope

This is the largest ticket in the backlog. A reasonable implementation
strategy is:

### 1. Introduce a platform abstraction layer

Create `include/subprocess/detail/` with separate Unix and Windows
implementations behind a common interface, selected at compile time via
`#ifdef _WIN32`.

Key seams to abstract:
| Concept | Unix | Windows |
|---|---|---|
| Spawn | `fork` + `exec` | `CreateProcess` |
| Wait | `waitpid` | `WaitForSingleObject` |
| Pipe | `pipe(2)` | `CreatePipe` |
| Signal / terminate | `kill(2)` | `TerminateProcess` |
| Working directory | `chdir` | `lpCurrentDirectory` in `CreateProcess` |
| Set UID/GID | `setuid`/`setgid` | N/A (no direct equivalent) |
| Stream handles | fd-based `fdstream` | `HANDLE`-based streams |

### 2. Replace `PrepExec` with a cross-platform abstraction

`PrepExec` is deeply tied to `execvp`. On Windows, argument quoting rules
differ and `CreateProcess` takes a flat command string rather than an argv
array. Argument escaping for Windows (`CommandLineToArgvW` rules) must be
implemented carefully.

### 3. `Exec::shell()`

On Windows, `Exec::shell()` should use `cmd.exe /c` rather than `sh -c`.
The `shell()` implementation (ticket 10) should already have a stub for this.

### 4. `ExitStatus`

The `Signaled` variant does not apply on Windows. `WaitForSingleObject` /
`GetExitCodeProcess` return a `DWORD`. Map to `Exited` or `Other` as
appropriate.

### 5. Features with no Windows equivalent

- `setuid` / `setgid` / `setpgid` — `PopenConfig` fields should be
  ignored (or cause a `LogicError`) on Windows.
- `send_signal(SIGTERM)` / `send_signal(SIGKILL)` — map to
  `TerminateProcess` for `terminate()` and `kill()`; arbitrary signal
  numbers should return a `LogicError` on Windows.

## Notes

- Depends on all prior tickets being completed and the overall architecture
  being stable, since the abstraction layer touches every part of the
  implementation.
- Consider using a Windows CI runner (GitHub Actions `windows-latest`) from
  the outset so regressions are caught early.
- `boost::fdstream` (used for `std_in`/`std_out`/`std_err`) may need to be
  replaced or supplemented with a `HANDLE`-based stream on Windows.
