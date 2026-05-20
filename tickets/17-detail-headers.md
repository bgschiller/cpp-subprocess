# Move internal headers to `include/subprocess/detail/`

## Problem

The `include/subprocess/` directory contains both the public API headers and
several implementation-detail headers that are not part of the library's
intended API surface:

| Header | Why it is an internal detail |
|---|---|
| `posix.hpp` | Low-level OS helpers (`pipe`, `set_inheritable`, `panic`, `reset_sigpipe`, `decode_exit_status`) — none are part of the public API |
| `PrepExec.hpp` | Pre-fork exec preparation, only used by `Popen::do_exec()` |
| `RaggedCstrArray.hpp` | Null-terminated argv/envp builder, only used by `PrepExec` |
| `ChildState.hpp` | Internal state machine of `Popen`; should be invisible once ticket 16 is done |
| `variant_helpers.hpp` | `overloaded<>` and `variant_to_string()` — debugging utilities used in `.cpp` files and `toString()` methods |
| `type_name.hpp` | `get_type_name<T>()` — a compile-time name utility used only by `variant_helpers.hpp` |

Having them in the public directory:
- Implies they are part of the installed API
- Confuses new contributors trying to understand what the library exposes
- Bloats the installed header tree for downstream consumers
- Makes the path to Windows (ticket 12) and Pipeline (ticket 11) harder:
  contributors cannot tell which headers are safe to change without breaking
  callers

## Proposed changes

Create `include/subprocess/detail/` and move the six headers above into it.
Update all `#include` directives in `src/` and other headers that reference
them.  Update `cmake/SourcesAndHeaders.cmake` to list them separately (or not
list them at all — detail headers do not need to be in the public install set).

The public header directory then contains only:
```
include/subprocess/
    CaptureData.hpp
    Exec.hpp
    ExitStatus.hpp
    Popen.hpp
    PopenConfig.hpp
    PopenError.hpp
    Redirection.hpp
    Result.hpp
    SubprocessException.hpp
```

`NullFile` (currently defined inside `Exec.hpp`) stays there.

## Migration steps

1. Create `include/subprocess/detail/`.
2. Move the six headers (with `git mv` to preserve history).
3. Update include guards (e.g. `SUBPROCESS_POSIX_H_` → `SUBPROCESS_DETAIL_POSIX_H_`).
4. Update all `#include "subprocess/posix.hpp"` → `#include "subprocess/detail/posix.hpp"`, etc.
5. Remove detail headers from the `headers` list in `cmake/SourcesAndHeaders.cmake`
   (or add them to a separate `detail_headers` variable for the build but not
   the install).
6. Verify the installed tree by running `make install` and inspecting
   `~/.local/include/subprocess/`.

## Affected files

- All six headers listed above — moved, include guards updated
- `src/Popen.cpp`, `src/Exec.cpp`, `src/PrepExec.cpp`, `src/ChildState.cpp`,
  `src/Redirection.cpp`, `src/ExitStatus.cpp` — `#include` paths updated
- `include/subprocess/Popen.hpp` — includes `ChildState.hpp`, `PrepExec.hpp`,
  `posix.hpp` (transitively via vendor) — update paths
- `cmake/SourcesAndHeaders.cmake`

## Dependencies

- Should be done after ticket **16** (encapsulate `child_state`), so that
  `ChildState.hpp` is not needed in any external include chain before it moves.
- Ticket **12** (Windows) will be significantly easier if done after this,
  since the platform-abstraction layer naturally lives in `detail/`.
