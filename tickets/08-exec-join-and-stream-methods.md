# Implement `Exec::join()`, `stream_stdout()`, and `stream_stdin()`

## Problem

The `Exec` builder is missing three convenience run methods present in the
Rust crate. All three are straightforward wrappers around `Exec::popen()`.

## Proposed API

```cpp
// include/subprocess/Exec.hpp

/// Launch the process, wait for it to exit, and return its exit status.
Result<ExitStatus> join();

/// Launch the process and return its stdout stream.
///
/// The process's stdout must have been set to Redirection::Pipe (which
/// this method does automatically if it has not already been set).
/// The caller reads from the returned stream and the process runs
/// concurrently.
Result<boost::fdistream> stream_stdout();

/// Launch the process and return its stdin stream.
///
/// The process's stdin must have been set to Redirection::Pipe (which
/// this method does automatically if it has not already been set).
/// The caller writes to the returned stream; the process runs
/// concurrently.
Result<boost::fdostream> stream_stdin();
```

## Implementation notes

**`join()`:**
```cpp
Result<ExitStatus> Exec::join() {
    auto p = popen();
    if (!p.ok()) return p.take_error();
    return p.take_value().wait();
}
```

**`stream_stdout()`:**
- If `config.stdout` is `Redirection::None`, set it to `Redirection::Pipe()`
  before calling `popen()`.
- After `popen()` succeeds, move `std_out` out of the `Popen` and return it.
- The `Popen` object is intentionally discarded (detached) — the stream
  lifetime owns the interaction.

**`stream_stdin()`:**
- Mirror of `stream_stdout()` but for `config.stdin` / `std_in`.

## Affected files

- `include/subprocess/Exec.hpp`
- `src/Exec.cpp`

## Notes

- Depends on ticket **07** (`Exec::popen()`).
- The test stub in `test/src/exec.cpp` (currently just constructing an
  `Exec` with a TODO comment) should be expanded here to cover all three
  methods.
