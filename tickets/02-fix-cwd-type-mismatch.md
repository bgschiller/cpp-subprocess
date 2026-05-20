# Fix `cwd` type mismatch between `Exec` and `PopenConfig`

## Problem

`Exec::cwd()` accepts a `const std::filesystem::path&` (as declared in
`include/subprocess/Exec.hpp`), but `PopenConfig::cwd` is
`std::optional<std::string>`, and `Popen::do_exec` passes it to `chdir()`
as a raw C string. The conversion from `std::filesystem::path` to
`std::string` never happens — `Exec::cwd()` stores the path into
`PopenConfig::cwd` which expects a `std::string`.

Additionally, `Popen::do_exec`'s signature takes `cwd` as
`std::optional<std::string>`, so the mismatch runs all the way through the
call stack.

## Fix

Two acceptable approaches:

**Option A — Normalise on `std::filesystem::path` throughout:**
Change `PopenConfig::cwd` and `Popen::do_exec`'s parameter to
`std::optional<std::filesystem::path>`. This is the more correct approach
and lets the OS layer call `path::c_str()` directly.

**Option B — Convert at the `Exec` boundary:**
Keep `PopenConfig::cwd` as `std::optional<std::string>` and call
`.string()` on the path inside `Exec::cwd()` before storing it.

Option A is preferred as `std::filesystem::path` is already used in
`Redirection` and is more portable.

## Affected files

- `include/subprocess/PopenConfig.hpp`
- `include/subprocess/Exec.hpp`
- `src/Exec.cpp`
- `src/Popen.cpp`

## Notes

No dependencies on other tickets, but should be fixed early since any `Exec`
run method (see ticket 07) will exercise this path.
