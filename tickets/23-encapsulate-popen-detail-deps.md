# Encapsulate `Popen` detail-header dependencies

## Problem

After ticket 17, all internal headers live in `include/subprocess/detail/`.
However, `Popen.hpp` — a public header — still directly includes two of them:

```cpp
#include "detail/ChildState.hpp"   // for the `child_state` private member
#include "detail/PrepExec.hpp"     // for the `do_exec()` private method (Unix only)
```

This means:

- `detail/` must be installed alongside the public headers, undoing some of
  the benefit of the move.
- Any change to `ChildState.hpp` or `PrepExec.hpp` forces a recompilation of
  every translation unit that includes `Popen.hpp`.
- The `ChildState` and `PrepExec` types leak through the public header even
  though they are implementation details.

Additionally, `Popen.hpp` already includes `detail/platform.hpp` for
`pid_type`, but this is a genuine public need (users see `pid()` return
`pid_type`).  Platform shims are a legitimate use of `detail/` in a public
header.

## Proposed changes

### ChildState

Replace the `ChildState child_state` private member with a `std::unique_ptr`
to an opaque, forward-declared type (Pimpl idiom).  The implementation in
`src/Popen.cpp` would define and construct the real `ChildState`.

Concretely:

1. Forward-declare `struct ChildState;` in `Popen.hpp` (or use a
   `detail::ChildState` wrapper).
2. Replace `ChildState child_state;` with
   `std::unique_ptr<ChildState> child_state_;`.
3. All `child_state.is_a<>()`, `child_state.get<>()`, etc. calls in
   `Popen.cpp` remain the same.
4. Remove `#include "detail/ChildState.hpp"` from `Popen.hpp`.

This isolates `ChildState.hpp` entirely to `src/Popen.cpp` (and
`src/ChildState.cpp`).

### PrepExec

`PrepExec` appears only in the private `do_exec()` method signature, which is
behind `#ifndef _WIN32`.  Since it is only used as a parameter type in a
private method, it can be forward-declared:

```cpp
#ifndef _WIN32
class PrepExec;  // forward declaration
#endif
```

Then remove `#include "detail/PrepExec.hpp"` from `Popen.hpp`.  The
definition is already included in `src/Popen.cpp` (it is needed for the
implementation of `Popen::os_start` and `Popen::do_exec`).

### Platform shim

`#include "detail/platform.hpp"` stays in `Popen.hpp` because `pid_type` is
part of the public API (return type of `pid()`).

## After completion

Once `Popen.hpp` no longer includes any header from `detail/` (except
`platform.hpp`), the install step can be updated to exclude `detail/` from
the installed tree:

```cmake
install(
  DIRECTORY include/${PROJECT_NAME_LOWERCASE}
  DESTINATION include
  PATTERN "detail" EXCLUDE
  PATTERN "vendor" EXCLUDE
)
```

(This also removes the `vendor/` directory from the installed tree, which
is already a good idea.)

## Affected files

- `include/subprocess/Popen.hpp` — remove `ChildState.hpp` and `PrepExec.hpp`
  includes, add forward declarations, change member to `unique_ptr`
- `src/Popen.cpp` — include `detail/ChildState.hpp`, minor API adjustments
- `src/ChildState.cpp` — no changes expected
- `CMakeLists.txt` — add `PATTERN "detail" EXCLUDE` to install rule

## Dependencies

- Must be done **after** ticket 17 (move internal headers to `detail/`).
- Ticket **12** (Windows) will benefit from a cleaner `Popen.hpp`.
