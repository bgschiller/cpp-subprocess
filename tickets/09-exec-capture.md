# Implement `Exec::capture()`

## Problem

There is no way to use the `Exec` builder to run a process, feed it stdin
data, and collect its stdout and stderr in one call. The `CaptureData`
struct exists but is never populated anywhere.

## Proposed API

```cpp
// include/subprocess/Exec.hpp

/// Launch the process, write any buffered stdin data, collect all
/// stdout and stderr, wait for exit, and return the result.
///
/// If stdin data was supplied via `Exec::stdin(const std::string&)` or
/// `Exec::stdin(const std::vector<uint8_t>&)`, stdout and stderr are
/// both redirected to pipes automatically (if not already configured).
Result<CaptureData> capture();
```

## Implementation notes

- Before calling `popen()`, ensure `config.stdout` and `config.stderr` are
  set to `Redirection::Pipe()` if they are `Redirection::None`.
- Call `popen()` to get a `Popen`.
- Call `popen.communicate(stdin_data)` (from ticket 06), which handles
  concurrent I/O and returns when the process has exited.
- Construct and return a `CaptureData` from the collected stdout, stderr,
  and exit status.

## `CaptureData` changes needed

`CaptureData::stdout` and `CaptureData::stderr` are currently `const
std::string` with no constructor, making the struct hard to build.
Add an explicit constructor or change the fields to be non-const.

## Affected files

- `include/subprocess/CaptureData.hpp`
- `include/subprocess/Exec.hpp`
- `src/Exec.cpp`

## Notes

- Depends on ticket **06** (`Popen::communicate()`).
- Depends on ticket **07** (`Exec::popen()`).
