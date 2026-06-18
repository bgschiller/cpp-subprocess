# 04 — Add stream_stdout / stream_stdin to Pipeline

`Pipeline` supports `popen()`, `join()`, and `capture()` but lacks the
streaming convenience methods that `Exec` provides.

## Proposal

```cpp
class Pipeline {
    /// Launch all processes, detach the last Popen, and return its stdout as a
    /// readable stream.  If stdout of the last stage is not configured, it is
    /// automatically set to Pipe() before spawning.
    Result<boost::fdistream> stream_stdout();

    /// Launch all processes, detach the first Popen, and return its stdin as a
    /// writable stream.  If stdin of the first stage is not configured, it is
    /// automatically set to Pipe() before spawning.
    Result<boost::fdostream> stream_stdin();
};
```

### stream_stdout()
1. If `!stdout_override_`, set `stdout_override_ = Redirection::Pipe()`.
2. Call `popen()`; on error propagate.
3. Detach the last `Popen` in the result vector (so its destructor is a no-op
   while the caller holds the stream).
4. Return `std::move(*last.std_out)`.

### stream_stdin()
1. If `!stdin_override_`, set `stdin_override_ = Redirection::Pipe()`.
2. Call `popen()`; on error propagate.
3. Detach the first `Popen` in the result vector.
4. Return `std::move(*first.std_in)`.

## Acceptance

- `Pipeline::stream_stdout()` test: verify output can be read from a two-stage
  pipeline's stream (e.g. `echo hello | cat`).
- `Pipeline::stream_stdin()` test: verify data written to the stream reaches
  the first stage (e.g. write to `cat | grep foo` and verify grep matches).
- Auto-pipe behaviour: stream_stdout with no stdout override works.
- Error propagation: nonexistent command in any stage returns an error.
