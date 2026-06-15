#ifndef SUBPROCESS_OPERATORS_H_
#define SUBPROCESS_OPERATORS_H_

/// \file
/// \brief Shell-style operator overloads for process construction.
///
/// Including this header makes `operator|` (pipe chaining) available.
/// Users who prefer the explicit `.pipe()` builder interface can include
/// only `Pipeline.hpp`.
///
/// \note `operator<` and `operator>` for file redirection are defined
///       in this header as well (ticket 21), sharing the same precedence
///       analysis that makes `Exec::cmd("cat") < "in.txt" |
///       Exec::cmd("grep").arg("foo") > "out.txt"` parse correctly.

#include "subprocess/Pipeline.hpp"

namespace subprocess {

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

}  // namespace subprocess

#endif  // SUBPROCESS_OPERATORS_H_
