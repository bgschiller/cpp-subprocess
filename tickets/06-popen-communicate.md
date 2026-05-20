# Implement `Popen::communicate()`

## Problem

`Communicator.hpp` is an incomplete skeleton. `RawCommunicator::do_read` has
a `constexpr` array sized from a non-constant variable and no return
statement — it does not compile if instantiated. There is no `communicate()`
method on `Popen`.

Without `communicate()`, there is no safe way to both write to a child's
stdin and read from its stdout/stderr simultaneously: doing so naively risks
deadlock if the child's output buffer fills before it reads more input.

## Expected behaviour (matching the Rust crate)

```cpp
// Send optional input data to stdin, collect all stdout and stderr,
// wait for the process to exit, and return the captured output.
Result<CaptureData> communicate(std::optional<std::string> input = std::nullopt);

// Byte-oriented variant.
Result<CaptureData> communicate_bytes(
    std::optional<std::vector<uint8_t>> input = std::nullopt);
```

- If `input` is provided, `std_in` must have been set to `Redirection::Pipe`.
- Stdout and stderr are collected only if the corresponding stream was set to
  `Redirection::Pipe`; otherwise the respective field in `CaptureData` is
  empty.
- Deadlock must be avoided. On Unix the simplest correct approach is to use
  `poll(2)` or `select(2)` to multiplex writes to stdin with reads from
  stdout/stderr on the same thread. A threading approach (one thread per
  stream) is also acceptable.
- After all I/O is complete, call `wait()` and populate `CaptureData::exit_status`.

## Affected files

- `include/subprocess/Communicator.hpp` — rewrite or replace the skeleton
- `include/subprocess/Popen.hpp` — add `communicate()` declarations
- `include/subprocess/CaptureData.hpp` — change fields from `const` to
  allow construction (or add a constructor)
- `src/Popen.cpp` — implement `communicate()`

## Notes

- Depends on ticket **03** (destructor) to ensure streams are properly
  cleaned up if `communicate()` throws or returns early.
- `communicate()` is a prerequisite for `Exec::capture()` (ticket 09).
