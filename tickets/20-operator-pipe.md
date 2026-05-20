# Promote `operator|` to a required deliverable for shell-style pipe chaining

## Background

Ticket **11** specifies the `Pipeline` class and lists `operator|` between
`Exec` and `Pipeline` as a "nice-to-have".  For the library's primary
ergonomic goal — letting users write process pipelines that read like a
shell one-liner — `operator|` is not optional; it IS the user-facing API.
This ticket promotes it to a required deliverable, fills in the precise
specification, and identifies the header strategy.

## Goal

The following should compile and behave correctly:

```cpp
#include <subprocess/operators.hpp>

using subprocess::Exec;

// Two-stage pipeline, stdout of cat piped to stdin of grep
auto pl = Exec::cmd("cat").arg("file.txt") | Exec::cmd("grep").arg("foo");

// Launch and wait
auto exit = pl.join().or_throw();

// Launch and capture the final stage's stdout
auto data = pl.capture().or_throw();

// Three-stage pipeline
auto pl3 = Exec::cmd("cat").arg("file.txt")
         | Exec::cmd("grep").arg("foo")
         | Exec::cmd("wc").arg("-l");
auto exit3 = pl3.join().or_throw();
```

## Operator signatures

```cpp
// include/subprocess/operators.hpp

namespace subprocess {

  /// Connect two Exec stages with a pipe.
  /// Exec::stdout of `lhs` is wired to Exec::stdin of `rhs`.
  Pipeline operator|(Exec lhs, Exec rhs);

  /// Extend an existing pipeline with another stage.
  Pipeline operator|(Pipeline lhs, Exec rhs);

}  // namespace subprocess
```

Both operators return `Pipeline` by value.  `Pipeline::pipe(Exec)` (from
ticket 11) is the underlying implementation.

## C++ operator precedence — why this composes correctly

In C++, the bitwise-OR operator `|` has **lower** precedence than the
relational operators `<` and `>`.  Ticket **21** adds `operator<` and
`operator>` for file redirection.  Because of this precedence ordering,
mixed expressions like:

```cpp
Exec::cmd("cat") < "in.txt" | Exec::cmd("grep").arg("foo") > "out.txt"
```

parse as:

```
(Exec::cmd("cat") < "in.txt") | (Exec::cmd("grep").arg("foo") > "out.txt")
```

Step by step:
1. `Exec::cmd("cat") < "in.txt"` — returns `Exec` with stdin set to `in.txt`
2. `Exec::cmd("grep").arg("foo") > "out.txt"` — returns `Exec` with stdout
   set to `out.txt`
3. `result_1 | result_2` — returns `Pipeline` wiring result_1's stdout to
   result_2's stdin

This is exactly the shell interpretation of `cat < in.txt | grep foo > out.txt`.
No parentheses are needed; precedence does the right thing automatically.

Similarly, longer chains with `|` are left-associative, so:

```cpp
a | b | c > "out"
```
parses as:
```
((a | b) | (c > "out"))
```

which creates a three-stage pipeline with `c`'s stdout redirected to a file.

## Header strategy

Define the operators in a dedicated header:

```
include/subprocess/operators.hpp
```

`operators.hpp` includes `Pipeline.hpp` (which in turn includes `Exec.hpp`)
and defines the two free functions in `namespace subprocess`.  Users who want
the terse syntax `#include <subprocess/operators.hpp>`; users who prefer the
explicit `.pipe()` builder API can include only `Pipeline.hpp`.

The operator definitions live in `src/Pipeline.cpp` alongside the rest of the
`Pipeline` implementation (no extra `.cpp` file needed).

## Interaction with `Pipeline` methods

`operator|(Exec, Exec)` constructs a `Pipeline` by calling
`Pipeline(std::move(lhs), std::move(rhs))`.  `operator|(Pipeline, Exec)`
calls `lhs.pipe(std::move(rhs))` and returns the modified `Pipeline` by value
(or returns `lhs` by value after in-place mutation — either works since
`Pipeline::pipe` can be written to return `Pipeline&` or `Pipeline&&`).

## Affected files

- `include/subprocess/operators.hpp` — new file; declares both operators
- `include/subprocess/Pipeline.hpp` — new file (from ticket 11); declares
  `Pipeline` class; included by `operators.hpp`
- `src/Pipeline.cpp` — new file (from ticket 11); implements `Pipeline` and
  the two operators
- `cmake/SourcesAndHeaders.cmake` — add `operators.hpp` to public headers
- `test/src/pipeline.cpp` — new file; tests for operator chaining (the
  manually-constructed two-process pipeline test in `simple_commands.cpp`
  should be converted to use `operator|`)

## Acceptance criteria

- `Exec | Exec` compiles and returns `Pipeline`.
- `Pipeline | Exec` compiles and returns `Pipeline`.
- Three-or-more stage chains compile without parentheses.
- The converted pipeline test in `simple_commands.cpp` (currently manual)
  passes using the operator syntax.
- `operator|` is NOT in scope when only `Exec.hpp` or `Pipeline.hpp` is
  included; it requires `operators.hpp`.

## Dependencies

- **Ticket 11** (`Pipeline`) must be implemented first; this ticket only adds
  the operator sugar and the dedicated header.
- **Ticket 19** (rename `Exec::stdin/stdout/stderr`) must be done before
  `Pipeline` is implemented to avoid having to rename again after `Pipeline`
  adds its own `set_stdin/set_stdout/set_stderr`.
- Ticket **21** (file redirection operators) is a sibling ticket; they share
  `operators.hpp` and the same precedence analysis.
