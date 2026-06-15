# subprocess

A C++17 library for spawning and managing child processes, modelled on the
[Rust `subprocess` crate](https://docs.rs/subprocess).

## Features

- **`Popen`** — low-level handle to a child process. Supports pipes for
  stdin/stdout/stderr, `wait`, `poll`, and `wait_timeout`.
- **`Exec`** — fluent builder for configuring and launching a process with
  argument lists, environment variables, working-directory overrides, and I/O
  redirection.
- **`Result<T>`** — lightweight error-or-value type used throughout the API
  instead of exceptions.
- **`Redirection`** — flexible I/O routing: pipes, file descriptors, paths
  (read/write/append), `Merge` (`2>&1`), or inherited.
- **`ExitStatus`** — discriminated union of `Exited`, `Signaled`, `Other`, and
  `Undetermined`.

## Requirements

- **CMake ≥ 3.15**
- **C++17** compiler (GCC, Clang)
- POSIX platform (Linux, macOS) — Windows support is planned

## Building

```bash
# Library only
cmake -Bbuild -DCMAKE_INSTALL_PREFIX=~/.local
cmake --build build --config Release

# Install
cmake --build build --target install --config Release
```

### Running the tests

```bash
cmake -Bbuild -DSubprocess_ENABLE_UNIT_TESTING=1
cmake --build build --config Release
cd build && ctest -C Release -VV
```

Or with the `Makefile` shorthand (cleans `build/` first):

```bash
make test
```

## Usage

All types live in the `subprocess` namespace. Link against `Subprocess::Subprocess`.

### Run a command and wait for it

```cpp
#include "subprocess/Popen.hpp"

auto result = subprocess::Popen::create({"echo", "hello"}, subprocess::PopenConfig{});
if (result.ok()) {
    auto p = result.take_value();
    auto exit = p.wait().or_throw();
    assert(exit.success());
}
```

### Capture output with a pipe

```cpp
subprocess::PopenConfig cfg;
cfg.stdout = subprocess::Redirection::Pipe();

auto p = subprocess::Popen::create({"echo", "yolo"}, cfg).or_throw();

std::string line;
std::getline(*p.std_out, line);   // line == "yolo"

p.wait().or_throw();
```

### Write to stdin and read from stdout

```cpp
subprocess::PopenConfig cfg;
cfg.stdin  = subprocess::Redirection::Pipe();
cfg.stdout = subprocess::Redirection::Pipe();

auto grep = subprocess::Popen::create({"grep", "apple"}, cfg).or_throw();

*grep.std_in << "apple\nbanana\npineapple\nlemon\n";
grep.std_in->close();

auto exit = grep.wait().or_throw();
assert(exit.success());

std::string output = grep.std_out->slurp();
// output == "apple\npineapple\n"
```

### Redirect stdin from a file

```cpp
subprocess::PopenConfig cfg;
cfg.stdin  = subprocess::Redirection::Read("input.txt").or_throw();
cfg.stdout = subprocess::Redirection::Pipe();

auto p = subprocess::Popen::create({"grep", "foo"}, cfg).or_throw();
p.wait().or_throw();
```

### Two-process pipeline

```cpp
auto [read_fd, write_fd] = subprocess::pipe().or_throw();  // returns std::tuple<int,int>

subprocess::PopenConfig cat_cfg, grep_cfg;
cat_cfg.stdout = subprocess::Redirection::FileDescriptor(write_fd);
grep_cfg.stdin  = subprocess::Redirection::FileDescriptor(read_fd);
grep_cfg.stdout = subprocess::Redirection::Pipe();

auto cat  = subprocess::Popen::create({"cat", "data.txt"}, cat_cfg).or_throw();
auto grep = subprocess::Popen::create({"grep", "sp"},      grep_cfg).or_throw();

// Close the parent's copies of the pipe ends
::close(read_fd);
::close(write_fd);

cat.wait().or_throw();
grep.wait().or_throw();

std::string out = grep.std_out->slurp();
```

### Check exit status

```cpp
auto p = subprocess::Popen::create({"/bin/sh", "-c", "exit 42"}, {}).or_throw();
auto status = p.wait().or_throw();

if (status.is_a<subprocess::ExitStatus::Exited>()) {
    int code = status.get<subprocess::ExitStatus::Exited>().code;  // 42
}
if (status.is_a<subprocess::ExitStatus::Signaled>()) {
    int sig = status.get<subprocess::ExitStatus::Signaled>().signal;
}
```

## API reference

### `Popen`

| Method | Description |
|---|---|
| `Popen::create(argv, config)` | Spawn a child process; returns `Result<Popen>` |
| `wait()` | Block until the process exits; returns `Result<ExitStatus>` |
| `poll()` | Non-blocking check; returns `std::optional<ExitStatus>` |
| `wait_timeout(ms)` | Block up to `ms` milliseconds; returns `Result<optional<ExitStatus>>` |
| `exit_status()` | Return cached exit status without a syscall |
| `pid()` | Return the child PID if still running |
| `std_in`, `std_out`, `std_err` | `std::optional<boost::fdostream/fdistream>` pipe streams |

### `PopenConfig`

| Field | Type | Default | Description |
|---|---|---|---|
| `stdin` | `Redirection` | `None` | Child's standard input |
| `stdout` | `Redirection` | `None` | Child's standard output |
| `stderr` | `Redirection` | `None` | Child's standard error |
| `detached` | `bool` | `false` | Don't reap child on `Popen` destruction |
| `executable` | `optional<string>` | `nullopt` | Override `argv[0]` as the executed binary |
| `env` | `optional<vector<EnvVar>>` | `nullopt` | Full environment (`nullopt` = inherit) |
| `cwd` | `optional<string>` | `nullopt` | Working directory (`nullopt` = inherit) |
| `setuid` | `optional<uid_t>` | `nullopt` | Drop to this UID before exec |
| `setgid` | `optional<gid_t>` | `nullopt` | Drop to this GID before exec |
| `setpgid` | `bool` | `false` | Call `setpgid(0,0)` before exec |

### `Redirection`

| Factory | Description |
|---|---|
| `Redirection::None()` | Inherit from parent (default) |
| `Redirection::Pipe()` | Create a pipe; parent end available as `std_in/out/err` |
| `Redirection::Merge()` | Merge with the other output stream (`2>&1`) |
| `Redirection::FileDescriptor(fd)` | Use an existing file descriptor |
| `Redirection::Read(path)` | Open a file for reading and pass it as stdin |
| `Redirection::Write(path)` | Open a file for writing (truncate) |
| `Redirection::Append(path)` | Open a file for writing (append) |

### `Result<T>`

| Method | Description |
|---|---|
| `ok()` | Returns `true` if holding a value |
| `take_value()` | Move the success value out |
| `take_error()` | Move the `PopenError` out |
| `or_throw()` | Return value or throw `SubprocessException` |

### `Exec` builder *(in progress)*

`Exec::cmd("program").arg("--flag").cwd("/tmp").set_stdout(Redirection::Pipe())`

| Method | Description |
|---|---|
| `Exec::cmd(command)` | Construct an `Exec` for `command` |
| `arg(s)` / `add_args(v)` | Append argument(s) |
| `env(k, v)` / `env_extend(v)` / `env_clear()` / `env_remove(k)` | Manage environment |
| `cwd(path)` | Set working directory |
| `set_stdin/set_stdout/set_stderr(r)` | Configure I/O redirection |
| `detached()` | Mark the process as detached |

`Exec::popen()`, `join()`, `capture()`, `stream_stdout()`, `stream_stdin()`, and
`Exec::shell()` are currently being implemented (see `tickets/`).

## Project status

The core `Popen` API is complete and tested. The `Exec` builder and `Pipeline`
type are under active development. See the `tickets/` directory for the full
backlog.
