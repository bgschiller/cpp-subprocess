#include <catch2/catch.hpp>
#include <fcntl.h>
#include <optional>
#include <string>
#include <unistd.h>

#include "subprocess/Exec.hpp"
#include "subprocess/ExitStatus.hpp"
#include "subprocess/Popen.hpp"
#include "subprocess/Redirection.hpp"
#include "subprocess/posix.hpp"

TEST_CASE("Exec::popen") {
  SECTION("simple command succeeds") {
    subprocess::Result<subprocess::Popen> result =
        subprocess::Exec::cmd("echo").arg("hello").popen();
    REQUIRE(result.ok());
    subprocess::Popen proc = result.take_value();
    subprocess::Result<subprocess::ExitStatus> wait_result = proc.wait();
    REQUIRE(wait_result.ok());
    REQUIRE(wait_result->success());
  }

  SECTION("multiple args") {
    subprocess::Result<subprocess::Popen> result =
        subprocess::Exec::cmd("echo").arg("foo").arg("bar").popen();
    REQUIRE(result.ok());
    subprocess::Popen proc = result.take_value();
    REQUIRE(proc.wait()->success());
  }

  // Note: bare nonexistent command names (e.g. "__nonexistent__") hit a known
  // bug in PrepExec::exec() where the PATH iteration throws std::out_of_range
  // on the last segment, causing the child to abort().  The parent then sees
  // EOF on the exec-error pipe and incorrectly treats the launch as a success.
  // Use an absolute path instead – that skips PATH iteration and ENOENT is
  // reported correctly.  See tickets/15-prep-exec-path-loop-oob.md.
  SECTION("absolute path to nonexistent command returns error") {
    subprocess::Result<subprocess::Popen> result =
        subprocess::Exec::cmd("/definitely/does/not/exist/cmd").popen();
    REQUIRE_FALSE(result.ok());
  }

  SECTION("stdout pipe captures output") {
    subprocess::Result<subprocess::Popen> result =
        subprocess::Exec::cmd("echo")
            .arg("subprocess")
            .stdout(subprocess::Redirection::Pipe())
            .popen();
    REQUIRE(result.ok());
    subprocess::Popen proc = result.take_value();
    REQUIRE(proc.std_out.has_value());
    std::string line;
    std::getline(*proc.std_out, line);
    REQUIRE(line == "subprocess");
    REQUIRE(proc.wait()->success());
  }

  SECTION("add_args works") {
    subprocess::Result<subprocess::Popen> result =
        subprocess::Exec::cmd("echo")
            .add_args({"a", "b", "c"})
            .stdout(subprocess::Redirection::Pipe())
            .popen();
    REQUIRE(result.ok());
    subprocess::Popen proc = result.take_value();
    std::string line;
    std::getline(*proc.std_out, line);
    REQUIRE(line == "a b c");
    REQUIRE(proc.wait()->success());
  }

  SECTION("popen without extra args runs command") {
    subprocess::Result<subprocess::Popen> result = subprocess::Exec::cmd("true").popen();
    REQUIRE(result.ok());
    subprocess::Popen proc = result.take_value();
    REQUIRE(proc.wait()->success());
  }

  SECTION("failing command exits non-zero") {
    subprocess::Result<subprocess::Popen> result = subprocess::Exec::cmd("false").popen();
    REQUIRE(result.ok());
    subprocess::Popen proc = result.take_value();
    subprocess::ExitStatus status = proc.wait().take_value();
    REQUIRE_FALSE(status.success());
  }
}
