# Rename `Exec::stdin/stdout/stderr` to avoid `<stdio.h>` macro collisions

## Problem

`Exec` declares builder methods named `stdin`, `stdout`, and `stderr`:

```cpp
Exec& stdin(Redirection capture) &;
Exec& stdout(Redirection capture) &;
Exec& stderr(Redirection capture) &;
// …and their &&, NullFile, string, and vector<uint8_t> overloads
```

`<stdio.h>` (C standard library) defines `stdin`, `stdout`, and `stderr` as
preprocessor macros.  Because `Popen.hpp` includes `<stdio.h>` and `Exec.hpp`
includes `Popen.hpp`, these macros are active when the compiler parses the
`Exec` class body.

The preprocessor expands macro names **before** parsing, so every occurrence
of `stdin` in the class body — including the method declarations — is
substituted with whatever the macro expands to.  On different platforms:

| Platform | `stdin` expands to |
|---|---|
| Linux (glibc) | `stdin` (self-referential no-op — harmless) |
| macOS | `__stdinp` (the actual FILE pointer) |
| Windows (MSVC) | `(__acrt_iob_func(0))` |

On macOS the method is effectively declared as:

```cpp
Exec& __stdinp(Redirection capture) &;
```

This compiles and works because **both** the declaration and the call site
expand the macro identically.  However:

- Debuggers and stack traces show `__stdinp`, `__stdoutp`, `__stderrp`
  instead of `stdin`, `stdout`, `stderr` — confusing.
- Any code that `#undef`s the macros between the declaration and the call
  site (unusual but legal) would produce a name mismatch and a linker error.
- On Windows (`__acrt_iob_func(0)`) the expansion contains parentheses and
  cannot form a valid identifier — the build would break outright.
- The behaviour is platform-dependent and fragile; it is only tolerated today
  because the self-referential glibc definition hides the problem on Linux CI.

## Proposed rename

| Current name | New name |
|---|---|
| `Exec::stdin(...)` | `Exec::set_stdin(...)` |
| `Exec::stdout(...)` | `Exec::set_stdout(...)` |
| `Exec::stderr(...)` | `Exec::set_stderr(...)` |

The `set_` prefix is consistent with the setter convention used throughout
the class (`env`, `cwd`, `detached` are fine as-is because they are not
macro names).

Alternatively, `#undef stdin` / `#undef stdout` / `#undef stderr` could be
inserted in `Exec.hpp` before the class body.  This is shorter but
potentially surprising to users who need those macros after including
`Exec.hpp`.  The rename is cleaner.

## Also: remove unnecessary `#include <stdio.h>` from `Popen.hpp`

`Popen.hpp` includes `<stdio.h>` but does not use any symbol from it directly
(neither `FILE*`, `printf`, nor `fopen` appear in the header).  Removing this
include eliminates the macro source for the common case and is good hygiene
regardless of the rename.

## Impact

- All call sites of `Exec::stdin/stdout/stderr` must be updated (library
  source, tests, README examples).
- This is a **breaking public API change**.  It should be paired with a
  minor or major version bump.
- The `NullFile` struct and its documentation already reference `stdin`,
  `stdout`, `stderr` as method names — these doc comments must be updated too.

## Affected files

- `include/subprocess/Exec.hpp` — rename declarations; remove `#include <stdio.h>` indirectly via Popen.hpp
- `include/subprocess/Popen.hpp` — remove `#include <stdio.h>`
- `src/Exec.cpp` — rename definitions
- `test/src/exec.cpp` — update all call sites
- `test/src/simple_commands.cpp` — update any call sites
- `README.md` — update examples

## Dependencies

- Independent of all other tickets.
- Should be done before `Pipeline` (ticket 11) to avoid having to rename
  again after `Pipeline::stdin/stdout/stderr` are added with the same bug.
