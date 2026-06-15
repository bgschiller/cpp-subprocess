#ifndef SUBPROCESS_OPERATORS_H_
#define SUBPROCESS_OPERATORS_H_

/// \file
/// \brief Shell-style operator overloads for process construction.
///
/// Including this header makes `operator|` (pipe chaining) and
/// `operator<` / `operator>` / `operator>>` (file redirection) available.
/// Users who prefer the explicit builder interface can include only
/// `Pipeline.hpp` or `Exec.hpp` directly.
///
/// ## Operator precedence
///
/// In C++, `<` and `>` have **higher** precedence than `|`, so common
/// shell-style expressions parse correctly without parentheses:
///
/// | Expression | C++ parse | Shell equivalent |
/// |---|---|---|
/// | `a < "in" \| b` | `(a < "in") \| b` | `a < in \| b` |
/// | `a \| b > "out"` | `a \| (b > "out")` | `a \| b > out` |
/// | `a < "in" \| b > "out"` | `(a < "in") \| (b > "out")` | `a < in \| b > out` |

#include "subprocess/Pipeline.hpp"

namespace subprocess {

  // ── Pipe operator ────────────────────────────────────────────────────────
  // (left-to-right associativity; lowest relative precedence)

  /// Connect two `Exec` stages with a pipe.
  ///
  /// Stdout of `lhs` is wired to stdin of `rhs`.  Returns a two-stage
  /// `Pipeline`.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto data = (Exec::cmd("cat").arg("file.txt") |
  ///              Exec::cmd("grep").arg("foo"))
  ///                .capture()
  ///                .or_throw();
  /// ```
  Pipeline operator|(Exec lhs, Exec rhs);

  /// Extend an existing `Pipeline` with another `Exec` stage.
  ///
  /// Stdout of the last stage in `lhs` is wired to stdin of `rhs`.
  /// Returns the extended `Pipeline` by value.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto pl3 = Exec::cmd("cat").arg("file.txt")
  ///          | Exec::cmd("grep").arg("foo")
  ///          | Exec::cmd("wc").arg("-l");
  /// auto exit = pl3.join().or_throw();
  /// ```
  Pipeline operator|(Pipeline lhs, Exec rhs);

  // ── File redirection operators ───────────────────────────────────────────
  // (higher precedence than |, so `cmd < "in" | cmd > "out"` parses as
  //  `(cmd < "in") | (cmd > "out")` without parentheses.)

  /// Redirect stdin of `lhs` to read from the file at `rhs`.
  ///
  /// Equivalent to `lhs.set_stdin(Redirection::Read(rhs))`.  If the file
  /// cannot be opened, the error is deferred: the returned `Exec` carries it
  /// internally, and `popen()` (or `join()`, `capture()`, etc.) will return
  /// the error without spawning a child.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto data = (Exec::cmd("grep").arg("foo") < "input.txt")
  ///                .capture()
  ///                .or_throw();
  /// ```
  Exec operator<(Exec lhs, const std::filesystem::path& rhs);

  /// Redirect stdout of `lhs` to write (overwrite) the file at `rhs`.
  ///
  /// Equivalent to `lhs.set_stdout(Redirection::Write(rhs))`.  Errors are
  /// deferred to `popen()`.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// Exec::cmd("ls").arg("-la") > "listing.txt";
  /// ```
  Exec operator>(Exec lhs, const std::filesystem::path& rhs);

  /// Redirect stdout of `lhs` to append to the file at `rhs`.
  ///
  /// Equivalent to `lhs.set_stdout(Redirection::Append(rhs))`.  Errors are
  /// deferred to `popen()`.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// Exec::cmd("echo").arg("hello") >> "log.txt";
  /// ```
  Exec operator>>(Exec lhs, const std::filesystem::path& rhs);

  /// Redirect stdin of the *first* stage of `lhs` to read from `rhs`.
  ///
  /// Equivalent to `lhs.set_stdin(Redirection::Read(rhs))`.  Errors are
  /// deferred to `popen()`.
  Pipeline operator<(Pipeline lhs, const std::filesystem::path& rhs);

  /// Redirect stdout of the *last* stage of `lhs` to write (overwrite) `rhs`.
  ///
  /// Equivalent to `lhs.set_stdout(Redirection::Write(rhs))`.  Errors are
  /// deferred to `popen()`.
  Pipeline operator>(Pipeline lhs, const std::filesystem::path& rhs);

  /// Redirect stdout of the *last* stage of `lhs` to append to `rhs`.
  ///
  /// Equivalent to `lhs.set_stdout(Redirection::Append(rhs))`.  Errors are
  /// deferred to `popen()`.
  Pipeline operator>>(Pipeline lhs, const std::filesystem::path& rhs);

}  // namespace subprocess

#endif  // SUBPROCESS_OPERATORS_H_
