# Implement `Exec::popen()`

## Problem

The `Exec` builder can fully configure a subprocess but has no method to
actually launch it. `Exec::popen()` is the core run method from which all
other `Exec` run methods are derived.

## Proposed API

```cpp
// include/subprocess/Exec.hpp

/// Launch the configured process and return a `Popen` handle.
///
/// stdin data previously supplied via `Exec::stdin(const std::string&)`
/// or `Exec::stdin(const std::vector<uint8_t>&)` is *not* written here;
/// use `capture()` for that. This method simply spawns the process with
/// the configured redirections and returns control to the caller.
Result<Popen> popen();
```

## Implementation notes

- Construct a `PopenConfig` from the builder's stored fields (`config`,
  which is already a `PopenConfig`) and the command + args, then call
  `Popen::create()`.
- The `command` field maps to `PopenConfig::executable` when it differs
  from `argv[0]`; for `Exec::cmd()` they are the same.
- `stdin_data` is intentionally not consumed here — it is used by
  `capture()` (ticket 09).

## Affected files

- `include/subprocess/Exec.hpp`
- `src/Exec.cpp`

## Notes

- Depends on ticket **02** (cwd type mismatch) being resolved so that the
  `PopenConfig` produced here has a correctly typed `cwd`.
- All other `Exec` run methods (tickets 08, 09, 10) depend on this ticket.
