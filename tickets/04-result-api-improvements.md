# Improve the `Result<T>` API

## Problem

The `Result<T>` type in `include/subprocess/Result.hpp` is used pervasively
but is missing conveniences that make error handling readable, and has one
outright bug.

### 1. `or_throw()` leaks memory

```cpp
T&& or_throw() {
    if (!ok()) throw new SubprocessException(take_error().message.c_str());
    //               ^^^ throws a pointer — caller must delete it
    return take_value();
}
```

`throw new Foo` is an anti-pattern in C++. The exception should be thrown
by value: `throw SubprocessException(...)`.

### 2. No monadic chaining

Every call site must manually check `.ok()` and return early, producing
repetitive boilerplate. The Rust crate's ergonomics come largely from
`?`-style chaining. A minimal C++ equivalent:

- **`map(F)`** — transform the success value, propagate error unchanged
- **`and_then(F)`** — chain a fallible operation (F returns `Result<U>`)

### 3. No `operator*` / `operator->`

Accessing the value requires the verbose `.take_value()`. An `operator*`
(consuming or non-consuming) and `operator->` would reduce noise at call
sites, following the pattern of `std::optional`.

### 4. No `operator bool`

`if (result)` is more readable than `if (result.ok())`.

## Fix

- Change `throw new` to `throw` in `or_throw()`.
- Add `operator bool`, `operator*` (non-consuming reference), `operator->`.
- Add `map()` and `and_then()` template methods.

## Affected files

- `include/subprocess/Result.hpp`
- Optionally tidy up call sites in `src/Popen.cpp` to use the new API.

## Notes

- No hard dependencies, but completing this ticket first makes the code for
  tickets 06–09 significantly cleaner to write.
