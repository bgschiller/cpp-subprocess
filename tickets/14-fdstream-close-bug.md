# Fix `fdistream::close()` incorrect `dynamic_cast`

## Problem

In `include/vendor/fdstream.hpp`, the `fdistream::close()` method contains:

```cpp
void close() {
  dynamic_cast<fdoutbuf*>(rdbuf())->close();
}
```

`fdistream`'s stream buffer is a `fdinbuf`, **not** a `fdoutbuf`.  The two
classes are unrelated (neither derives from the other), so the `dynamic_cast`
always returns `nullptr`.  Calling `->close()` on the resulting null pointer
is undefined behaviour — in practice a segfault.

The correct cast is:

```cpp
void close() {
  dynamic_cast<fdinbuf*>(rdbuf())->close();
}
```

## Impact

`fdistream::close()` is not currently called anywhere in the library (streams
are closed by destroying the `optional` that wraps them, which invokes the
`fdinbuf` destructor correctly).  The bug is therefore latent rather than
actively harmful today, but it is a trap for any future caller.

## Fix

Change `fdoutbuf*` to `fdinbuf*` in `fdistream::close()`.

Add a test that constructs a temporary pipe, wraps the read end in
`fdistream`, calls `close()`, and verifies the fd is closed (e.g. a
subsequent `read()` fails with `EBADF`).

## Affected files

- `include/vendor/fdstream.hpp`
