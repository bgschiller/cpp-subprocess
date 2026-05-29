#ifdef _WIN32
#  define _CRT_SECURE_NO_WARNINGS
#endif
#include <catch2/catch.hpp>

#include "subprocess/Exec.hpp"
#include "subprocess/Pipeline.hpp"
#include "subprocess/Redirection.hpp"

// ---------------------------------------------------------------------------
// Pipeline constructor and pipe()
// ---------------------------------------------------------------------------

TEST_CASE("Pipeline: two-stage join") {
  // cat /dev/null | true  — both succeed
  auto status =
      subprocess::Pipeline(subprocess::Exec::cmd("cat").arg("/dev/null"), subprocess::Exec::cmd("true"))
          .join()
          .or_throw();
  REQUIRE(status.success());
}

TEST_CASE("Pipeline: operator| convenience") {
  auto status =
      (subprocess::Exec::cmd("echo").arg("hello") | subprocess::Exec::cmd("grep").arg("hello"))
          .join()
          .or_throw();
  REQUIRE(status.success());
}

TEST_CASE("Pipeline: three-stage operator|") {
  // echo "apple\nbanana" | grep apple | grep -v banana
  auto status =
      (subprocess::Exec::shell("printf 'apple\\nbanana\\n'") |
       subprocess::Exec::cmd("grep").arg("apple") |
       subprocess::Exec::cmd("grep").arg("-v").arg("banana"))
          .join()
          .or_throw();
  REQUIRE(status.success());
}

TEST_CASE("Pipeline: pipe() extends the chain") {
  subprocess::Pipeline pl(
      subprocess::Exec::cmd("echo").arg("hello"), subprocess::Exec::cmd("cat"));
  pl.pipe(subprocess::Exec::cmd("grep").arg("hello"));

  auto status = pl.join().or_throw();
  REQUIRE(status.success());
}

// ---------------------------------------------------------------------------
// Pipeline::capture()
// ---------------------------------------------------------------------------

TEST_CASE("Pipeline: capture stdout of last stage") {
  // echo "hello world" | cat  → stdout == "hello world\n"
  auto data =
      (subprocess::Exec::cmd("echo").arg("hello world") | subprocess::Exec::cmd("cat"))
          .capture()
          .or_throw();
  REQUIRE(data.out == "hello world\n");
  REQUIRE(data.success());
}

TEST_CASE("Pipeline: capture replaces existing two-process test") {
  // This replicates the manual pipe test in simple_commands.cpp using Pipeline.
  FILE* veggies = fopen("veggies2.tmp", "w");
  fprintf(veggies, "brussels sprouts\nkale\ncarrots\nbroccoli\ncauliflower\neggplant\nspinach\n");
  fclose(veggies);

  auto data =
      (subprocess::Exec::cmd("cat").arg("veggies2.tmp") |
       subprocess::Exec::cmd("grep").arg("sp"))
          .capture()
          .or_throw();

  REQUIRE(data.out == "brussels sprouts\nspinach\n");
  REQUIRE(data.success());
}

TEST_CASE("Pipeline: capture stderr from a stage") {
  // first stage writes to stderr; second stage is cat (passes through nothing
  // on stdout since first stage writes nothing there).
  auto data =
      (subprocess::Exec::shell("echo err_msg >&2") | subprocess::Exec::cmd("cat"))
          .capture()
          .or_throw();
  REQUIRE(data.err.find("err_msg") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Pipeline::popen() — direct handle access
// ---------------------------------------------------------------------------

TEST_CASE("Pipeline: popen returns handles for all stages") {
  auto procs =
      (subprocess::Exec::cmd("echo").arg("hi") | subprocess::Exec::cmd("cat"))
          .popen()
          .or_throw();
  REQUIRE(procs.size() == 2);
  for (auto& p : procs) {
    p.wait().or_throw();
  }
}

// ---------------------------------------------------------------------------
// stdin / stdout / stderr overrides
// ---------------------------------------------------------------------------

TEST_CASE("Pipeline: stdin(NullFile) makes first stage read from /dev/null") {
  // cat reads from /dev/null (empty input), so grep finds nothing → exit 1.
  auto status = (subprocess::Exec::cmd("cat") | subprocess::Exec::cmd("grep").arg("anything"))
                    .stdin_(subprocess::NullFile{})
                    .join()
                    .or_throw();
  REQUIRE_FALSE(status.success());
}

TEST_CASE("Pipeline: stdout(NullFile) discards last stage output") {
  auto status =
      (subprocess::Exec::cmd("echo").arg("hello") | subprocess::Exec::cmd("cat"))
          .stdout_(subprocess::NullFile{})
          .join()
          .or_throw();
  REQUIRE(status.success());
}
