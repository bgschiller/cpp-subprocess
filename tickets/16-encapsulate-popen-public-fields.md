# Encapsulate `Popen::child_state` and `Popen::detached`

## Problem

`Popen` exposes two `public` data members that are implementation details:

```cpp
ChildState child_state;   // the internal state machine
bool detached;            // destructor behaviour flag
```

**`child_state`** leaks the entire three-state internal machine
(`Preparing` / `Running` / `Finished`) to callers who shouldn't need to know
about it.  The public API already provides the right primitives — `pid()`,
`poll()`, `exit_status()`, `wait()` — and the state machine is an
implementation concern.  Making it public also allows external mutation, which
can corrupt invariants (e.g. setting `child_state` to `Finished` while the
process is still running would cause `wait()` to return a stale value without
calling `waitpid`).

**`detached`** is used externally by `Exec::stream_stdout()` and
`Exec::stream_stdin()`:

```cpp
proc.detached = true;   // reached into Popen's internals
return std::move(*proc.std_out);
```

This exposes a boolean that should be toggled via an internal API, not a
public field.

## Proposed changes

### Make `child_state` private

Remove the `public:` visibility on `child_state`.  All internal methods of
`Popen` already have access.  No external code should need to read or write it.

### Make `detached` private; add a private setter

Keep `detached` private.  Add a private method `void set_detached(bool)` (or
simply let `Exec` set it via friendship if preferred).  `Exec` can be declared
a `friend` so it can call the setter without making it public:

```cpp
friend class Exec;
```

Or add a public `detach()` method (no argument, one-way toggle) that matches
the builder API semantics — once detached you cannot un-detach.

The `Exec::detached()` builder method already writes `config.detached = true`
and this flows into the `Popen` constructor via `PopenConfig` — that path is
fine.  The in-flight `proc.detached = true` in `stream_stdout`/`stream_stdin`
is the only site that breaks the encapsulation.

### `std_in`, `std_out`, `std_err` public fields

These three `optional<fdstream>` public fields are intentionally part of the
public API (callers read/write to the streams), so they remain public.

## Impact

- `test/src/simple_commands.cpp` — a few tests currently inspect
  `child_state` directly (e.g. checking `is_a<ChildState::Running>()`).
  They should be rewritten to use `pid()` / `poll()` / `exit_status()`.
- `src/Exec.cpp` — the `proc.detached = true` lines become `proc.detach()`
  or a friend access.
- `include/subprocess/ChildState.hpp` — can be moved to `detail/` once this
  ticket and ticket 17 are done together.

## Affected files

- `include/subprocess/Popen.hpp` — move fields to `private:`, add
  `detach()` or friend declaration
- `src/Popen.cpp` — no functional changes (already uses the fields internally)
- `src/Exec.cpp` — use the new accessor
- `test/src/simple_commands.cpp` — replace direct `child_state` access

## Dependencies

- Can be done independently of other tickets.
- Should be coordinated with ticket **17** (detail headers), since
  `ChildState.hpp` could move to `detail/` at the same time.
