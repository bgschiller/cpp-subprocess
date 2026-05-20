# `operator<`, `operator>`, `operator>>` — file redirection operators

## Goal

Enable bash-style file redirection on `Exec` and `Pipeline` objects so users
can write:

```cpp
#include <subprocess/operators.hpp>

using subprocess::Exec;

// stdin from file
Exec::cmd("grep").arg("foo") < "input.txt";

// stdout to file (overwrite)
Exec::cmd("ls").arg("-la") > "listing.txt";

// stdout to file (append)
Exec::cmd("echo").arg("hello") >> "log.txt";

// Combined with pipe (ticket 20) — no parentheses needed
auto result = (Exec::cmd("cat") < "in.txt"
             | Exec::cmd("grep").arg("foo")
             | Exec::cmd("wc").arg("-l") > "count.txt")
              .join().or_throw();

// Redirect a fully-constructed pipeline's endpoints
auto pl = Exec::cmd("cat") | Exec::cmd("sort") | Exec::cmd("uniq");
auto data = (pl < "words.txt" > "deduped.txt").join().or_throw();
```

## Operator signatures

All operators are free functions in `namespace subprocess`, defined in
`operators.hpp` / `src/Pipeline.cpp`.

```cpp
namespace subprocess {

  // ── Exec redirection ─────────────────────────────────────────────────────

  /// Set stdin of `lhs` to read from the file at `rhs`.
  /// Equivalent to Exec::set_stdin(Redirection::Read(rhs)).
  Exec operator<(Exec lhs, const std::filesystem::path& rhs);

  /// Set stdout of `lhs` to write (overwrite) the file at `rhs`.
  /// Equivalent to Exec::set_stdout(Redirection::Write(rhs)).
  Exec operator>(Exec lhs, const std::filesystem::path& rhs);

  /// Set stdout of `lhs` to append to the file at `rhs`.
  /// Equivalent to Exec::set_stdout(Redirection::Append(rhs)).
  Exec operator>>(Exec lhs, const std::filesystem::path& rhs);

  // ── Pipeline redirection ─────────────────────────────────────────────────

  /// Set stdin of the *first* stage of `lhs` to read from the file at `rhs`.
  Pipeline operator<(Pipeline lhs, const std::filesystem::path& rhs);

  /// Set stdout of the *last* stage of `lhs` to write (overwrite) `rhs`.
  Pipeline operator>(Pipeline lhs, const std::filesystem::path& rhs);

  /// Set stdout of the *last* stage of `lhs` to append to `rhs`.
  Pipeline operator>>(Pipeline lhs, const std::filesystem::path& rhs);

}  // namespace subprocess
```

String literals and `std::string` are implicitly convertible to
`std::filesystem::path`, so users can write `cmd > "out.txt"` without a cast.

## The deferred-error problem and its solution

`Redirection::Read`, `Redirection::Write`, and `Redirection::Append` return
`Result<Redirection>` because opening the file can fail (`ENOENT`,
`EACCES`, …).  The redirection operators must return `Exec` / `Pipeline`
by value (not `Result<Exec>`) so the chaining syntax stays clean.

The solution is to let `Exec` (and `Pipeline`) carry a deferred error:

```cpp
// Exec.hpp — new private field
std::optional<PopenError> deferred_error_;
```

When an operator fails to open the file it stores the error in
`deferred_error_` instead of returning immediately.  `Exec::popen()` (and
`Exec::join()`, `Exec::capture()`, etc.) check this field first and return
the stored error without spawning anything.

```cpp
// Exec::popen() preamble
if (deferred_error_) {
    return *deferred_error_;
}
```

`Pipeline` needs the same field, checked at the start of `Pipeline::popen()`.

Implementation of `operator>` on `Exec`:

```cpp
Exec operator>(Exec lhs, const std::filesystem::path& rhs) {
    if (lhs.deferred_error_) return lhs;          // propagate earlier error
    auto r = Redirection::Write(rhs);
    if (!r.ok()) {
        lhs.deferred_error_ = r.take_error();
        return lhs;
    }
    lhs.set_stdout(r.take_value());               // renamed per ticket 19
    return lhs;
}
```

The same pattern applies to all six operators.

### Why not throw?

The library uses `Result<T>` throughout and avoids exceptions as a control
flow mechanism.  Throwing from `operator>` would be inconsistent.

### Why not return `Result<Exec>`?

```cpp
// This would not compile because Result<Exec> has no operator|:
Exec::cmd("cat") < "in.txt" | Exec::cmd("grep") > "out.txt"
//                 ^^^^^^^^ Result<Exec>, which has no operator|
```

Deferred errors keep the expression syntax intact while preserving the
`Result`-based error contract at the point of actual execution (`popen()`).

## C++ operator precedence — the expressions parse correctly

In C++, `<` and `>` have **higher** precedence than `|`.  See ticket 20 for
the full precedence table.  The key consequence:

| Expression | C++ parse | Shell equivalent |
|---|---|---|
| `a < "in" \| b` | `(a < "in") \| b` | `a < in \| b` ✓ |
| `a \| b > "out"` | `a \| (b > "out")` | `a \| b > out` ✓ |
| `a < "in" \| b > "out"` | `(a < "in") \| (b > "out")` | `a < in \| b > out` ✓ |
| `a \| b \| c > "out"` | `a \| b \| (c > "out")` | `a \| b \| c > out` ✓ |

No parentheses are needed for any of the common cases.

Note that `operator>>` in C++ has the same precedence as `>>` in shift
expressions (higher than `|`), so it also composes correctly:
`a | b >> "log"` parses as `a | (b >> "log")`.

## `stderr` redirection

Bash's `2>` and `2>>` have no natural C++ operator equivalent (`2>` is not a
valid operator token).  The `2>&1` merge (`Redirection::Merge`) is also
inexpressible as an operator.  These redirection forms are left to the
explicit builder API (`Exec::set_stderr(Redirection::Write(path))`).  This is
a deliberate scope boundary — the operators cover the common 80 % case; the
full `set_stdin/set_stdout/set_stderr` API covers everything else.

## Interaction with the `Pipeline` operator

When a `Pipeline` overload is applied (e.g. `pl > "out.txt"`), it must
configure the stdout of the **last stage** already stored in the pipeline's
`std::vector<Exec>`.  This requires `Pipeline` to expose a way to mutate the
last `Exec` in the internal vector.  One clean approach:

```cpp
// Pipeline — private helper used by the operators
void set_last_stdout(Redirection r);
void set_first_stdin(Redirection r);
```

These are called directly from the operator implementations (which are
`friend` free functions or live in the same translation unit as `Pipeline`).

Alternatively, `Pipeline::pipe(Exec)` already takes the `Exec` by value, so
the vector stores `Exec` objects directly and can be mutated without extra
indirection.

## Affected files

- `include/subprocess/operators.hpp` — add six new operator declarations
  (shared with ticket 20)
- `include/subprocess/Exec.hpp` — add `std::optional<PopenError>
  deferred_error_` (private); add `friend` declarations for the operator
  functions, or make `deferred_error_` accessible via a private setter
- `include/subprocess/Pipeline.hpp` — add `std::optional<PopenError>
  deferred_error_` (private); add private helpers
  `set_last_stdout` / `set_first_stdin`
- `src/Pipeline.cpp` — implement the six operator functions
- `src/Exec.cpp` — add deferred-error check to `popen()`, `join()`,
  `capture()`, `stream_stdout()`, `stream_stdin()`
- `test/src/pipeline.cpp` — tests for all six operators, including error
  propagation and multi-stage expressions

## Acceptance criteria

- `Exec::cmd("cmd") < "path"` compiles and returns `Exec`.
- `Exec::cmd("cmd") > "path"` compiles and returns `Exec`.
- `Exec::cmd("cmd") >> "path"` compiles and returns `Exec`.
- Same for `Pipeline lhs`.
- A missing input file causes `popen()` (not the operator itself) to return
  a `PopenError::IoError`.
- A deferred error on `Exec` survives being piped: `(bad_exec < "missing") | other_exec`
  produces a `Pipeline` whose `popen()` returns an error.
- All six operators are only in scope when `operators.hpp` is included.
- The expressions in the **Goal** section above compile and produce correct
  results at runtime.

## Dependencies

- **Ticket 11** (`Pipeline`) — `operator<`/`>`/`>>` on `Pipeline` require
  the `Pipeline` class to exist.
- **Ticket 19** (rename `Exec::stdin/stdout/stderr` → `set_stdin/set_stdout/
  set_stderr`) — the operators call `set_stdout`/`set_stdin`; the rename must
  happen first to avoid the `<stdio.h>` macro collision on Windows.
- **Ticket 20** (`operator|`) — sibling ticket; both share `operators.hpp`.
  Can be implemented in the same PR.
