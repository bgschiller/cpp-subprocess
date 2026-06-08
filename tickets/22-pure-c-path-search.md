# Port PATH search in `PrepExec::exec()` to pure C

## Problem

`PrepExec::exec()` currently iterates over PATH components using C++ `std::string`
members (`find`, `at`, `operator[]`, `size`) in the child process **after
`fork()`**.  Any C++ exception thrown here (e.g. `std::out_of_range`) is
uncaught and causes `std::terminate()` → `abort()`.  Ticket 15 patched the
immediate OOB bug, but a more robust fix is to avoid C++ exceptions entirely in
the post-fork path.

## Proposed fix

Rewrite the PATH search using only C primitives:

- Use `strtok_r` (or manual pointer arithmetic) on the raw `searchpath` C-string
  to walk through `:`-delimited segments.
- Build the executable path with `snprintf` into `prealloc_exe`.
- No `std::optional`, no `std::string::find`, no `std::string::at` — nothing
  that can throw.

## Affected files

- `src/PrepExec.cpp` — the `exec()` method and possibly parts of the constructor

## Notes

- The `PrepExec` constructor already has access to the raw `PATH` environment
  variable via `std::getenv("PATH")`.  It could store the C-string pointer or
  a copy for use in `exec()`.
- The `prealloc_exe` buffer is already pre-sized in the constructor; verify that
  the sizing remains correct after refactoring.
- Tests in `test/src/exec.cpp` (especially the "bare nonexistent command"
  section) should continue to pass without changes.
