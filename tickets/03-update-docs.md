# 03 — Update README and AGENTS.md to reflect completion

The project docs claim `Exec` and `Pipeline` are "in progress" / "planned,"
but both are fully implemented and tested.

## Changes needed: README.md

- "Exec builder *(in progress)*" → remove "(in progress)".  Document that
  `Exec::popen()`, `join()`, `capture()`, `stream_stdout()`, `stream_stdin()`,
  and `Exec::shell()` are all available.
- "Project status" section: replace "under active development" with a
  statement reflecting the v0.1.0 feature set.
- Add `Pipeline` to the features list with a two-line description.

## Changes needed: AGENTS.md

- `Pipeline` entry: remove "(planned, see tickets/11-pipeline.md)" — replace
  with a brief description of the completed type.
- Test file table: add entries for `pipeline_test.cpp`, `fdstream_test.cpp`,
  `variant_visit_test.cpp`, `windows_diagnostic_test.cpp`.  Remove `Communicator.hpp`
  row (file no longer exists; communicate logic lives in `Popen.cpp`).

## Acceptance

Both files accurately describe the current state of the library with no
stale "planned" / "in progress" markers for completed features.
