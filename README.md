[![Actions Status](https://github.com/filipdutescu/modern-cpp-template/workflows/MacOS/badge.svg)](https://github.com/filipdutescu/modern-cpp-template/actions)
[![Actions Status](https://github.com/filipdutescu/modern-cpp-template/workflows/Windows/badge.svg)](https://github.com/filipdutescu/modern-cpp-template/actions)
[![Actions Status](https://github.com/filipdutescu/modern-cpp-template/workflows/Ubuntu/badge.svg)](https://github.com/filipdutescu/modern-cpp-template/actions)
[![codecov](https://codecov.io/gh/filipdutescu/modern-cpp-template/branch/master/graph/badge.svg)](https://codecov.io/gh/filipdutescu/modern-cpp-template)
[![GitHub release (latest by date)](https://img.shields.io/github/v/release/filipdutescu/modern-cpp-template)](https://github.com/filipdutescu/modern-cpp-template/releases)

# Modern C++ Template

A quick C++ template for modern CMake projects, aimed to be an easy to use
starting point.

This is my personal take on such a type of template, thus I might not use the
best practices or you might disagree with how I do things. Any and all feedback
is greatly appreciated!

## Features

* Modern **CMake** configuration and project, which, to the best of my
knowledge, uses the best practices,

* An example of a **Clang-Format** config, inspired from the base *Google* model,
with minor tweaks. This is aimed only as a starting point, as coding style
is a subjective matter, everyone is free to either delete it (for the *LLVM*
default) or supply their own alternative,

* **Static analyzers** integration, with *Clang-Tidy* and *Cppcheck*, the former
being the default option,

* **Doxygen** support, through the `ENABLE_DOXYGEN` option, which you can enable
if you wish to use it,

* **Unit testing** support, through *GoogleTest* (with an option to enable
*GoogleMock*) or *Catch2*,

* **Code coverage**, enabled by using the `ENABLE_CODE_COVERAGE` option, through
*Codecov* CI integration,

* **Package manager support**, with *Conan* and *Vcpkg*, through their respective
options

* **CI workflows for Windows, Linux and MacOS** using *GitHub Actions*, making
use of the caching features, to ensure minimum run time,

* **.md templates** for: *README*, *Contributing Guideliness*,
*Issues* and *Pull Requests*,

* Options to build as a header-only library or executable, not just a static or
shared library.

* **Ccache** integration, for speeding up rebuild times

## Getting Started

These instructions will get you a copy of the project up and running on your local
machine for development and testing purposes.

### Prerequisites

* **CMake v3.15+** - found at [https://cmake.org/](https://cmake.org/)

* **C++ Compiler** - needs to support at least the **C++17** standard, i.e. *MSVC*,
*GCC*, *Clang*

> ***Note:*** *You also need to be able to provide ***CMake*** a supported
[generator](https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html).*

### Installing

Run the `install` target with CMake.
For example:

```bash
cmake --build build --target install --config Release

# a more general syntax for that command is:
cmake --build <build_directory> --target install --config <desired_config>
```

## Building the project

To build the project, all you need to do, ***after correctly
[installing the project](README.md#Installing)***, is run a similar **CMake** routine
to the the one below:

```bash
mkdir build/ && cd build/
cmake .. -DCMAKE_INSTALL_PREFIX=/absolute/path/to/custom/install/directory
cmake --build . --target install
```

> ***Note:*** *The custom ``CMAKE_INSTALL_PREFIX`` can be omitted if you wish to
install in [the default install location](https://cmake.org/cmake/help/latest/module/GNUInstallDirs.html).*

More options that you can set for the project can be found in the
[`cmake/StandardSettings.cmake` file](cmake/StandardSettings.cmake). For certain
options additional configuration may be needed in their respective `*.cmake` files
(i.e. Conan needs the `CONAN_REQUIRES` and might need the `CONAN_OPTIONS` to be setup
for it work correctly; the two are set in the [`cmake/Conan.cmake` file](cmake/Conan.cmake)).

## Generating the documentation

In order to generate documentation for the project, you need to configure the build
to use Doxygen. This is easily done, by modifying the workflow shown above as follows:

```bash
mkdir build/ && cd build/
cmake .. -D<project_name>_ENABLE_DOXYGEN=1 -DCMAKE_INSTALL_PREFIX=/absolute/path/to/custom/install/directory
cmake --build . --target doxygen-docs
```

> ***Note:*** *This will generate a `docs/` directory in the **project's root directory**.*

## Running the tests

By default, the template uses [Google Test](https://github.com/google/googletest/)
for unit testing. Unit testing can be disabled in the options, by setting the
`ENABLE_UNIT_TESTING` (from
[cmake/StandardSettings.cmake](cmake/StandardSettings.cmake)) to be false. To run
the tests, simply use CTest, from the build directory, passing the desire
configuration for which to run tests for. An example of this procedure is:

```bash
cd build          # if not in the build directory already
ctest -C Release  # or `ctest -C Debug` or any other configuration you wish to test

# you can also run tests with the `-VV` flag for a more verbose output (i.e.
#GoogleTest output as well)
```

### End to end tests

If applicable, should be presented here.

### Coding style tests

If applicable, should be presented here.
