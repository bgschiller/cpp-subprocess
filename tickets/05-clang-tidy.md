# 05 — Run clang-tidy and fix warnings

The project has a `.clang-tidy` config with all checks enabled and
`WarningsAsErrors`, but it has not been run on the current codebase.

## Steps

1. Build the project with `CMAKE_EXPORT_COMPILE_COMMANDS=ON`:
   ```bash
   cmake -Bbuild -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
   ```

2. Run clang-tidy over all source files:
   ```bash
   run-clang-tidy -p build -header-filter='include/subprocess' \
     src/ test/src/
   ```

   Or use the CMake target if available:
   ```bash
   cmake --build build --target clang-tidy
   ```

3. Fix every warning.  Common C++17 clang-tidy findings:
   - missing `noexcept` on move constructors / swap
   - `std::move` on const objects (no-op)
   - use of `()` vs `{}` for construction
   - missing `override` on virtual functions
   - implicit bool conversions
   - `std::string` parameters passed by value instead of `std::string_view`
   - modernise-* recommendations

4. Add clang-tidy to CI (optional follow-up):
   - Add a `run-clang-tidy` step to `.github/workflows/ubuntu.yml` that
     fails the build on warnings.

## Acceptance

- `run-clang-tidy` reports zero warnings across all source and header files.
- The project still compiles and all 57 test cases pass.
