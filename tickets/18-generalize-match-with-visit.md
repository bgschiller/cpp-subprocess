# Generalize `match()` to support arbitrary return types via `std::visit`

## Problem

`Redirection::match()` and `ChildState::match()` are hard-wired to return
`Result<const std::nullopt_t>`:

```cpp
// Redirection.hpp
Result<const std::nullopt_t> match(
    std::function<Result<const std::nullopt_t>(const Pipe&)>        pipe_case,
    std::function<Result<const std::nullopt_t>(const FileDescriptor&)> file_case,
    std::function<Result<const std::nullopt_t>(const Merge&)>       merge_case,
    std::function<Result<const std::nullopt_t>()>                   none_case) const;

// ChildState.hpp
Result<const std::nullopt_t> match(
    std::function<Result<const std::nullopt_t>(const Preparing&)> preparing_case,
    std::function<Result<const std::nullopt_t>(const Running&)>   running_case,
    std::function<Result<const std::nullopt_t>(const Finished&)>  finished_case) const;
```

This has two compounding problems:

1. **Fixed return type.**  Every call site that wants to extract a value
   must capture a local variable and mutate it from within a lambda, rather
   than returning the value directly.  The code becomes unnecessarily
   imperative and harder to read.

2. **`std::function` overhead.**  `std::function` type-erases the callable,
   which typically heap-allocates and virtual-dispatches.  A `std::visit`
   with a templated visitor is zero-overhead.

The `overloaded<>` helper in `variant_helpers.hpp` already exists to make
`std::visit` ergonomic:

```cpp
std::visit(overloaded{
    [](const Redirection::Pipe&)           { ... },
    [](const Redirection::FileDescriptor&) { ... },
    [](const Redirection::Merge&)          { ... },
    [](const Redirection::None&)           { ... },
}, redirection._state);   // _state is currently private
```

The only reason `match()` exists is that `_state` is private, so callers
cannot call `std::visit` directly.

## Proposed changes

### Add a public `visit()` method templated on the visitor

```cpp
// Redirection
template<typename Visitor>
auto visit(Visitor&& v) const {
    return std::visit(std::forward<Visitor>(v), _state);
}

// ChildState
template<typename Visitor>
auto visit(Visitor&& v) const {
    return std::visit(std::forward<Visitor>(v), _state);
}
```

Usage becomes:

```cpp
return redirection.visit(subprocess::internal::overloaded{
    [&](const Redirection::Pipe&)           -> Result<...> { ... },
    [&](const Redirection::FileDescriptor&) -> Result<...> { ... },
    [&](const Redirection::Merge&)          -> Result<...> { ... },
    [&](const Redirection::None&)           -> Result<...> { ... },
});
```

The return type is deduced from the visitor — it can be `Result<T>` for any `T`,
`bool`, `std::string`, `int`, or anything else.

### Deprecate and remove `match()`

Once all internal call sites are migrated to `visit()`, remove `match()` from
both classes.  The `std::function`-based implementation in the `.cpp` files
also goes away, shrinking the compiled library.

### `ExitStatus` has the same pattern

`ExitStatus` does not expose `match()`, but it does expose `is_a<T>()` and
`get<T>()`, which forces callers to write if/else chains.  Add the same
templated `visit()` to `ExitStatus` for consistency.

## Migration of existing call sites

`Popen::setup_streams` — currently uses `Redirection::match()` with three
large lambdas that capture `this` and local variables by reference.  With
`visit()` + `overloaded`, the structure and logic stay the same but the
return type constraint is lifted.

`Popen::waitpid` — currently uses `ChildState::match()`.  Same migration.

## Affected files

- `include/subprocess/Redirection.hpp` — add `visit()`, deprecate `match()`
- `include/subprocess/ChildState.hpp` — add `visit()`, deprecate `match()`
- `include/subprocess/ExitStatus.hpp` — add `visit()`
- `src/Redirection.cpp` — remove `match()` implementation (after migration)
- `src/ChildState.cpp` — remove `match()` implementation (after migration)
- `src/Popen.cpp` — migrate `setup_streams` and `waitpid` to `visit()`

## Dependencies

- `variant_helpers.hpp` already provides `overloaded<>` — no new utility code
  is needed.
- Can be done independently of other tickets, but coordinate with ticket **17**
  (detail headers) since `variant_helpers.hpp` will move at that time.
