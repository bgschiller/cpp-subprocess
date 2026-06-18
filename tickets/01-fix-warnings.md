# 01 — Fix -Wpessimizing-move warnings

Three locations return `std::move(Temporary{})` which prevents copy elision.
Clang warns with `-Wpessimizing-move`.

## Affected files

| File | Line(s) |
|---|---|
| `src/Popen.cpp:125` | `return std::move(boost::fdostream(parent_end));` |
| `src/Popen.cpp:135` | `return std::move(boost::fdistream(parent_end));` |
| `test/src/simple_commands.cpp:100` | `config.stdin_ = std::move(Redirection::FileDescriptor(fruitsFd));` |

## Fix

Remove the `std::move()` wrapper in all three places.  The compiler already
elides the copy; `std::move` pessimises by forcing a move-constructor call.

## Acceptance

- `cmake --build build` produces zero warnings.
- All 57 test cases (290 assertions) continue to pass.
