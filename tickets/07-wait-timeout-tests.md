# 07 — Add dedicated wait_timeout tests

`Popen::wait_timeout()` is exercised implicitly through `poll()` (which calls
`wait_timeout(0ms)`) but there are no tests verifying the timeout behaviour:

- That it actually returns `nullopt` (timed out) when the child hasn't exited.
- That it returns the exit status when the child finishes within the timeout.
- That it does not hang when the child runs longer than the timeout.

## Tests to add

| Test | Description |
|---|---|
| `wait_timeout returns nullopt when timeout expires` | Spawn `sleep 60`, call `wait_timeout(50ms)`, assert `nullopt`. Kill process after. |
| `wait_timeout returns exit status when child finishes quickly` | Spawn `true`, call `wait_timeout(5000ms)`, assert status is `Exited{0}`. |
| `wait_timeout on already-finished process returns immediately` | Spawn `true`, call `wait()` first, then `wait_timeout(0ms)`, assert status. |
| `wait_timeout minimum timeout (0ms)` | Same as `poll()` but explicit — verify it returns immediately with either `nullopt` (still running) or status. |

## Acceptance

- All four sections pass on both Unix and Windows.
- Processes are cleaned up (killed/wait) even when timeouts happen.
