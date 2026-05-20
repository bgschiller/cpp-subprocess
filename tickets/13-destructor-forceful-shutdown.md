# Graceful + forceful shutdown in the `Popen` destructor

## Problem

The current `Popen` destructor closes open pipe ends and then calls `wait()`
unconditionally.  If the child process ignores EOF on stdin and never exits on
its own, the destructor blocks forever.  This is a silent hang with no
diagnostic and no escape hatch.

The Rust `subprocess` crate handles this by sending `SIGTERM` (and optionally
`SIGKILL` after a grace period) before waiting, giving the child a chance to
shut down cleanly before being forcibly killed.

## Proposed behaviour

Add a `PopenConfig` field that controls destructor policy:

```cpp
enum class DestructorPolicy {
  /// Wait indefinitely for the child to exit (current behaviour).
  Wait,
  /// Send SIGTERM, then wait indefinitely.
  Terminate,
  /// Send SIGTERM, wait up to a configurable grace period, then SIGKILL.
  TerminateAndKill,
};
```

`PopenConfig` gains two new fields:

```cpp
/// What the destructor does when the child is still running.
DestructorPolicy destructor_policy{ DestructorPolicy::Wait };

/// Grace period between SIGTERM and SIGKILL when using TerminateAndKill.
std::chrono::milliseconds kill_grace_period{ std::chrono::milliseconds(3000) };
```

The destructor logic becomes:

```
close pipes
if child is still running:
    switch destructor_policy:
        Wait:             wait() [existing behaviour]
        Terminate:        send_signal(SIGTERM); wait()
        TerminateAndKill: send_signal(SIGTERM)
                          wait_timeout(kill_grace_period)
                          if still running: send_signal(SIGKILL); wait()
```

Errors from signal delivery and wait are silently swallowed (destructor
context).

## Affected files

- `include/subprocess/PopenConfig.hpp` — new enum + fields
- `include/subprocess/Popen.hpp` — store policy; update destructor doc
- `src/Popen.cpp` — implement the new destructor logic
- `test/src/simple_commands.cpp` — tests for each policy

## Dependencies

- Ticket **05** (`terminate()` / `kill()` / `send_signal()`) must be
  implemented first so the destructor can call `send_signal()` internally.
- The `wait_timeout` path already exists and can be reused directly.

## Notes

- `detached = true` continues to bypass the destructor entirely (no signals,
  no wait).
- On Windows, `SIGTERM` is replaced by `TerminateProcess()`; leave a
  `#ifdef _WIN32` stub consistent with ticket **12**.
