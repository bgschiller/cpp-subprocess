# Fix out-of-bounds read in `PrepExec::exec()` PATH iteration

## Problem

`PrepExec::exec()` iterates over PATH components to find the executable.  When
processing the **last** component (which has no trailing `:`), `end` is set to
`std::string::npos` (= `SIZE_MAX`).  The inner loop condition

```cpp
while (start < end) {
    prealloc_exe[ix++] = searchpath->at(start++);
}
```

compares `start` against `SIZE_MAX`, which is always true for any reasonable
`start` value.  As `start` advances past the end of the string,
`std::string::at()` throws `std::out_of_range`.

Because this code runs **after `fork()`** in the child process and there is no
`try`/`catch` around `do_exec()`, the exception is uncaught and causes
`std::terminate()` → `abort()`.  When the child aborts:

1. The write end of the exec-error pipe is closed by the OS.
2. The parent's `::read()` from the exec-error pipe returns **0** (EOF).
3. The parent interprets `readCnt == 0` as "exec succeeded" and returns
   `Ok(Popen)` even though the command was never found.

The net effect is that `Popen::create` (and therefore `Exec::popen()`) silently
succeeds for any non-existent command that is looked up via PATH.  Callers only
discover the failure when `wait()` returns a signalled (SIGABRT) exit status.

## Steps to reproduce

```cpp
auto r = subprocess::Exec::cmd("__definitely_does_not_exist__").popen();
assert(r.ok());                          // incorrectly true
assert(!r->wait()->success());           // exits via SIGABRT
```

Commands specified as absolute paths (e.g.
`/definitely/does/not/exist/cmd`) are **not** affected because they bypass the
PATH loop entirely.

## Fix

Replace the inner loop that copies the current PATH segment into `prealloc_exe`
so that it stops at the correct boundary.  The simplest fix is to use the
actual string length as the upper bound:

```cpp
size_t seg_end = (end == std::string::npos) ? searchpath->size() : end;
while (start < seg_end) {
    prealloc_exe[ix++] = (*searchpath)[start++];
}
```

After this change, the last PATH segment is copied correctly, `libc_exec()` is
called and returns `ENOENT`, and the errno is written to the exec-error pipe so
the parent can return a proper error.

## Affected files

- `src/PrepExec.cpp`
- `test/src/exec.cpp` — the "bare nonexistent command returns error" test (currently
  commented out due to this bug) can be re-enabled.

## Notes

- This is a post-fork bug; heap allocation and C++ exceptions should be avoided
  entirely in the child after `fork()`.  A safer long-term fix is to port the
  PATH search to pure C (using `strtok_r` or manual pointer arithmetic) so that
  no C++ exceptions can be thrown in the child.
- Ticket discovered while implementing `Exec::popen()` (ticket 07).
