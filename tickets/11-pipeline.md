# Implement `Pipeline`

## Problem

The Rust subprocess crate's `Pipeline` type, which connects two or more
`Exec` configurations into a shell-style pipe chain, has no equivalent here.
`NullFile`'s documentation and `Exec.hpp`'s doc comments already reference
`Pipeline`, but the class does not exist.

## Proposed API

```cpp
// include/subprocess/Pipeline.hpp

class Pipeline {
public:
    /// Create a pipeline from two Exec configurations.
    ///
    ///   auto pl = Pipeline(Exec::cmd("cat"), Exec::cmd("grep").arg("foo"));
    Pipeline(Exec first, Exec second);

    /// Extend the pipeline with another stage.
    Pipeline& pipe(Exec next);

    /// Configure stdin of the first process in the pipeline.
    Pipeline& stdin(Redirection r);
    Pipeline& stdin(NullFile);

    /// Configure stdout of the last process in the pipeline.
    Pipeline& stdout(Redirection r);
    Pipeline& stdout(NullFile);

    /// Configure stderr for all processes, or the last one only.
    Pipeline& stderr(Redirection r);
    Pipeline& stderr(NullFile);

    /// Launch all processes in the pipeline and return their Popen handles.
    Result<std::vector<Popen>> popen();

    /// Launch, wait for all processes, return last exit status.
    Result<ExitStatus> join();

    /// Launch, collect stdout of the last process and stderr of all.
    Result<CaptureData> capture();
};
```

A `|` operator overload between `Exec` and `Pipeline` is a nice-to-have:

```cpp
Pipeline operator|(Exec lhs, Exec rhs);
Pipeline operator|(Pipeline lhs, Exec rhs);
```

## Implementation notes

- Internally store a `std::vector<Exec>`.
- `popen()` must create the inter-process pipes before spawning any child,
  then spawn in order. The write end of each pipe becomes the stdout of
  process N, and the read end becomes the stdin of process N+1.
- Close both ends of each intermediate pipe in the parent after spawning,
  to avoid keeping the write end open (which would prevent the reader from
  seeing EOF).
- The existing two-process pipeline test in `simple_commands.cpp` does this
  manually — use it as a reference and convert it to a `Pipeline`-based test.

## Affected files

- `include/subprocess/Pipeline.hpp` — new file
- `src/Pipeline.cpp` — new file
- `cmake/SourcesAndHeaders.cmake` — add new files
- `test/src/exec.cpp` or a new `test/src/pipeline.cpp` — tests

## Notes

- Depends on ticket **07** (`Exec::popen()`).
- Depends on ticket **09** (`Exec::capture()`) for `Pipeline::capture()`.
- This is the most complex ticket in the non-Windows category; budget
  accordingly.
