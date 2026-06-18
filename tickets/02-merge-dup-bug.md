# 02 — Fix Merge redirection fd leak (dup bug)

The TODO at `src/Popen.cpp:507` identifies a real bug.

## Problem

When `MergeKind::ErrToOut` is set, `child_stderr` is assigned the same fd
number as `child_stdout`.  Inside `Popen::do_exec()`, `dup2()` + `close()`
is called for stdout first, then for stderr.  Since `close()` destroys the
original fd after the first `dup2`, the second `dup2` receives a stale
(closed) fd as its source — `dup2(closed_fd, 2)` returns EBADF.

The bug triggers when:

- stderr is `Redirection::Merge` (2>&1) AND stdout is `Pipe`,
  `FileDescriptor`, or any non-inherited redirection (i.e. `child_stdout != 1`).
  
- OR stdout is `Redirection::Merge` (1>&2) AND stderr is any non-inherited
  redirection.

When the target stream is inherited from the parent (`child_std{out,err} == 1`
or `== 2`), the `!= N` check in `do_exec()` skips the `dup2`, so no bug occurs.
This is why existing tests pass: no test exercises Merge with a piped target.

## Fix

In `Popen::setup_streams()`, when `merge != MergeKind::None`, call `dup()`
on the target fd so each stream gets an independent copy:

```cpp
if (merge == MergeKind::ErrToOut) {
    if (child_stdout != 0 && child_stdout != 1 && child_stdout != 2) {
        child_stderr = ::dup(child_stdout);  // fresh copy for stderr
    } else {
        child_stderr = child_stdout;         // inherited std fds are safe
    }
}
// Analogous for MergeKind::OutToErr.
```

## Acceptance

- A test that spawns `sh -c 'echo out; echo err >&2'` with `stdout=Pipe`,
  `stderr=Merge` and verifies that stderr appears in stdout (merged), not lost.
- A test that spawns `sh -c 'echo out; echo err >&2'` with `stdout=Merge`,
  `stderr=Pipe` and verifies that stdout appears in stderr.
- All existing tests continue to pass.
