# 06 — Complete Windows fdstream_test

`test/src/fdstream_test.cpp` tests `boost::fdistream::close()` only on Unix.
The Windows branch is a stub:

```cpp
#else
  // Windows fdstream close tests are not yet implemented.
  REQUIRE(true);
#endif
```

## Goal

Verify that `fdistream::close()` works on Windows, parallel to the Unix test:

1. Create a pipe via `CreatePipe` or `_pipe()`.
2. Write a byte so the read end has data.
3. Wrap the read end in `boost::fdistream`.
4. Call `stream.close()`.
5. Assert that a subsequent read from the same fd fails (the fd is closed).

## Notes

- On Windows, `boost::fdistream` wraps a CRT fd obtained via `_open_osfhandle`.
  We need to verify the close path works correctly for both `CreatePipe`-based
  and `_pipe`-based fds.
- Alternatively, use `subprocess::pipe()` (which already abstracts the
  platform) to create the pipe, then pass one end to `boost::fdistream`.

## Acceptance

- The test verifies `fdistream::close()` on Windows.
- Test compiles and passes on a Windows CI run.
