#ifndef SUBPROCESS_RESULT_H_
#define SUBPROCESS_RESULT_H_

#include <string>
#include <variant>

#include "PopenError.hpp"
#include "SubprocessException.hpp"

namespace subprocess {

  /// @brief Lightweight error-or-value type used throughout the subprocess API.
  ///
  /// A `Result<T>` either holds a success value of type `T` or a `PopenError`
  /// describing what went wrong.  It is modelled loosely on the Rust `Result`
  /// type and provides monadic chaining via `map()` and `and_then()`.
  ///
  /// ### Basic usage
  /// ```cpp
  /// Result<int> r = some_operation();
  /// if (r) {                   // operator bool — true on success
  ///     int v = *r;            // operator* — reference to value
  ///     r->some_method();      // operator->
  /// } else {
  ///     PopenError e = r.take_error();
  /// }
  /// ```
  ///
  /// ### Chaining
  /// ```cpp
  /// auto result = open_file(path)
  ///     .and_then([](File f) { return read_all(f); })
  ///     .map([](std::string s) { return s.size(); });
  /// ```
  template<class T>
  class Result {
    using StateType = std::variant<PopenError, T>;
    StateType _state;

   public:
    /// Construct from anything that can initialise the underlying `variant`.
    template<typename... Args>
    Result(Args&&... args)
        : _state{ std::forward<Args>(args)... } { }

    Result(Result&& other)
        : _state{ std::move(other._state) } { }
    Result(const Result& other)
        : _state{ other._state } { }

    Result& operator=(Result&& other) {
      _state = std::move(other._state);
      return *this;
    }

    // ---------------------------------------------------------------------------
    // Observers
    // ---------------------------------------------------------------------------

    /// @returns `true` if this `Result` holds a success value.
    bool ok() const { return std::holds_alternative<T>(_state); }

    /// @returns `true` if this `Result` holds a success value.
    explicit operator bool() const { return ok(); }

    /// Dereference to a reference to the contained value.
    /// @pre `ok()` must be `true`; behaviour is undefined otherwise.
    T& operator*() { return std::get<T>(_state); }

    /// Dereference to a const reference to the contained value.
    /// @pre `ok()` must be `true`; behaviour is undefined otherwise.
    const T& operator*() const { return std::get<T>(_state); }

    /// Member-access operator delegating to the contained value.
    /// @pre `ok()` must be `true`; behaviour is undefined otherwise.
    T* operator->() { return &std::get<T>(_state); }

    /// Member-access operator delegating to the contained value (const).
    /// @pre `ok()` must be `true`; behaviour is undefined otherwise.
    const T* operator->() const { return &std::get<T>(_state); }

    // ---------------------------------------------------------------------------
    // Value / error extraction (consuming)
    // ---------------------------------------------------------------------------

    /// Move the error out of this `Result`.
    /// @pre `!ok()` must be `true`.
    PopenError&& take_error() { return std::move(std::get<PopenError>(_state)); }

    /// Move the value out of this `Result`.
    /// @pre `ok()` must be `true`.
    T&& take_value() { return std::move(std::get<T>(_state)); }

    /// Return the contained value, or throw `SubprocessException` on error.
    ///
    /// The exception is thrown **by value** (not by pointer), so callers can
    /// catch it with `catch (const subprocess::SubprocessException& e)`.
    T&& or_throw() {
      if (!ok()) throw SubprocessException(take_error().message.c_str());
      return take_value();
    }

    // ---------------------------------------------------------------------------
    // Monadic combinators
    // ---------------------------------------------------------------------------

    /// Transform the success value with `f`, propagating any error unchanged.
    ///
    /// @tparam F  Callable with signature `U f(T)` (or `U f(T&)` / `U f(T&&)`).
    /// @returns   `Result<U>` where `U` is the return type of `f`.
    ///
    /// Example:
    /// ```cpp
    /// Result<int> r{42};
    /// Result<std::string> s = r.map([](int x){ return std::to_string(x); });
    /// ```
    template<typename F>
    auto map(F&& f) -> Result<decltype(f(std::declval<T&>()))> {
      using U = decltype(f(std::declval<T&>()));
      if (!ok()) return take_error();
      return Result<U>{ f(std::get<T>(_state)) };
    }

    /// Chain a fallible operation: `f` receives the success value and returns a
    /// `Result<U>`.  If `*this` already holds an error, `f` is never called.
    ///
    /// @tparam F  Callable with signature `Result<U> f(T)`.
    /// @returns   The `Result<U>` produced by `f`, or the propagated error.
    ///
    /// Example:
    /// ```cpp
    /// Result<Popen> p = Popen::create({"ls"}, cfg)
    ///     .and_then([](Popen proc){ return proc.wait(); });
    /// ```
    template<typename F>
    auto and_then(F&& f) -> decltype(f(std::declval<T&>())) {
      using R = decltype(f(std::declval<T&>()));
      if (!ok()) return R{ take_error() };
      return f(std::get<T>(_state));
    }
  };

}  // namespace subprocess

#endif
