# Implement `Exec::shell()`

## Problem

`Exec::shell()` is documented and stubbed in `Exec.hpp` but commented out:

```cpp
// static Exec shell(std::string cmdstr);
```

It is the idiomatic way to run a shell command string, equivalent to the C
`system()` function but with controllable I/O and exit status.

## Proposed API

```cpp
// include/subprocess/Exec.hpp

/// Constructs a new `Exec` that runs `cmdstr` via the system shell.
///
/// On Unix, equivalent to `Exec::cmd("sh").arg("-c").arg(cmdstr)`.
///
/// Prefer `Exec::cmd(...).arg(...)` when arguments are known at compile
/// time. Only use `shell()` when you genuinely need shell features
/// (globbing, pipes in the string, etc.), and never interpolate
/// untrusted input into the command string.
static Exec shell(std::string cmdstr);
```

## Implementation notes

```cpp
Exec Exec::shell(std::string cmdstr) {
    return Exec::cmd("sh").arg("-c").add_args({std::move(cmdstr)});
}
```

On Windows (when that is eventually supported, see ticket 12) this should
use `cmd.exe /c` instead.

## Affected files

- `include/subprocess/Exec.hpp` — uncomment and update the declaration
- `src/Exec.cpp` — add the implementation

## Notes

- Depends on ticket **07** (`Exec::popen()`) for the method to be
  exercisable in tests, though the implementation itself is independent.
- Add a test that runs a simple shell expression (e.g. `echo hello world`)
  and verifies the output via `capture()` (ticket 09) or `stream_stdout()`
  (ticket 08).
