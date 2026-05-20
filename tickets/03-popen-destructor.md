# Implement `Popen` destructor

## Problem

`Popen` has no destructor. There is a TODO comment in `Popen.hpp`:

```cpp
// TODO: make a destructor that closes any open std_in, std_err, std_out
```

Without a destructor:
- The child's stdin pipe is never closed when the `Popen` goes out of scope,
  which can leave the child blocked waiting for input.
- If the process is not detached, the child becomes a zombie after it exits
  because no one calls `waitpid`.
- The `std_in`, `std_out`, `std_err` fdstreams hold open file descriptors that
  are leaked.

## Expected behaviour (matching the Rust crate)

- Close `std_in`, `std_out`, `std_err` if they are open.
- If `detached` is false and the child is still running, call `wait()` to
  reap the child. Log or silently ignore any error (we are in a destructor).
- If `detached` is true, do nothing — the caller has opted out of lifecycle
  management.

## Affected files

- `include/subprocess/Popen.hpp`
- `src/Popen.cpp`

## Notes

- No dependencies on other tickets.
- Once implemented, verify existing tests still pass (the destructor running
  on test `Popen` objects must not cause double-waits or assertion failures).
