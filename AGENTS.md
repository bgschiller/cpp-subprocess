# AGENTS.md — cpp-subprocess

## Project Overview

`cpp-subprocess` is a C++17 library for spawning and managing child processes,
modelled closely on the [Rust `subprocess` crate](https://docs.rs/subprocess).
It provides:

- **`Popen`** — low-level handle to a running child process (fork/exec, pipes,
  wait, poll, timeout).
- **`Exec`** — fluent builder for configuring and launching a process. Supports
  argument lists, environment manipulation, working-directory changes, and I/O
  redirection.
- **`Pipeline`** (planned, see `tickets/11-pipeline.md`) — shell-style chain of
  `Exec` stages connected by pipes.
- **`Result<T>`** — lightweight error-or-value type used throughout the API
  instead of exceptions.

---

## Repository Layout

```
include/subprocess/   Public headers (the library's API surface)
src/                  Implementation (.cpp files, one per header)
test/src/             Catch2 unit tests
cmake/                CMake helper modules
tickets/              Plaintext work-item specifications (numbered)
build/                Out-of-source build directory (git-ignored)
Makefile              Convenience wrapper around CMake targets
.clang-format         Code style (Google base, 100-column limit)
.clang-tidy           Static analysis config (all checks, warnings as errors)
```

Key headers and their roles:

| Header                    | Role                                               |
| ------------------------- | -------------------------------------------------- |
| `Popen.hpp` / `Popen.cpp` | Core process handle                                |
| `Exec.hpp` / `Exec.cpp`   | Builder API                                        |
| `Result.hpp`              | `Result<T>` error type                             |
| `PopenConfig.hpp`         | Configuration struct passed to `Popen::create`     |
| `Redirection.hpp`         | Enum for stdin/stdout/stderr routing               |
| `ExitStatus.hpp`          | Discriminated union: `Exited`, `Signaled`, `Other` |
| `CaptureData.hpp`         | Return type for captured stdout/stderr             |
| `Communicator.hpp`        | Placeholder (communicate logic lives in `Popen.cpp`) |
| `PrepExec.hpp`            | POSIX exec helpers                                 |

---

## Building

The project uses **CMake ≥ 3.15** and requires a **C++17** compiler (GCC, Clang,
or MSVC).

```bash
# Configure + build (library only)
cmake -Bbuild -DCMAKE_INSTALL_PREFIX=~/.local
cmake --build build --config Release

# Configure + build with tests enabled
cmake -Bbuild -DSubprocess_ENABLE_UNIT_TESTING=1
cmake --build build --config Release
```

Or use the `Makefile` shorthand (it cleans `build/` first every time):

```bash
make test      # configure, build, and run CTest
make install   # configure, build, and install to ~/.local
make docs      # configure, build, and open Doxygen output
make coverage  # build with gcov coverage instrumentation
```

The build artifact is a static library: `build/libSubprocess.a`.

---

## Running Tests

```bash
# After building with Subprocess_ENABLE_UNIT_TESTING=1:
cd build && ctest -C Release -VV
```

Test sources live in `test/src/`:

| File                         | Coverage                                             |
| ---------------------------- | ---------------------------------------------------- |
| `main.cpp`                   | Catch2 entry point (`CATCH_CONFIG_MAIN`)              |
| `simple_commands.cpp`        | End-to-end `Popen` / pipeline tests                  |
| `exec.cpp`                   | `Exec` builder tests (stub, expand as you implement) |
| `ragged_cstr_array_test.cpp` | `RaggedCstrArray` unit tests                         |
| `result_test.cpp`            | `Result<T>` unit tests                               |
| `type_name_test.cpp`         | `type_name<T>()` utility tests                       |

All new behaviour **must** have a corresponding test. Tests use **Catch2**
(`TEST_CASE` / `SECTION` / `REQUIRE`). Do not use GoogleTest — this project
uses Catch2 exclusively. Do not add a `using namespace` declaration at file
scope — qualify types explicitly (e.g., `subprocess::Exec`,
`subprocess::Popen`).

---

## Code Style

- **Standard:** C++17. Use `std::filesystem`, `std::optional`, `std::variant`,
  structured bindings freely.
- **Formatting:** enforced by `.clang-format` (Google base, 100-column limit).
  Run before committing:
  ```bash
  cmake --build build --target clang-format
  ```
- **Static analysis:** `.clang-tidy` runs all checks with `WarningsAsErrors`.
  Fix every warning before opening a PR.
- **Error handling:** use `Result<T>` — never throw raw pointers
  (`throw new Foo` is a bug; use `throw Foo`).
- **Naming:** `snake_case` for variables, functions, and methods; `PascalCase`
  for types and classes; `UPPER_SNAKE_CASE` for macros only.
- **Headers:** each public header must be self-contained and guarded with
  `#ifndef SUBPROCESS_<NAME>_H_` / `#define` / `#endif`.
- **Namespace:** all library code lives in `namespace subprocess { }`.
