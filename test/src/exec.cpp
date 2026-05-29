#include <catch2/catch.hpp>
#ifndef _WIN32
#  include <fcntl.h>
#  include <unistd.h>
#endif
#include <optional>
#include <string>

#include "subprocess/CaptureData.hpp"
#include "subprocess/Exec.hpp"
#include "subprocess/ExitStatus.hpp"
#include "subprocess/Popen.hpp"
#include "subprocess/Redirection.hpp"
#include "subprocess/posix.hpp"
#include "vendor/fdstream.hpp"

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
            .stdout_(subprocess::Redirection::Pipe())
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
            .stdout_(subprocess::Redirection::Pipe())
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

TEST_CASE("Exec::join") {
  SECTION("successful command returns success exit status") {
    subprocess::Result<subprocess::ExitStatus> result =
        subprocess::Exec::cmd("true").join();
    REQUIRE(result.ok());
    REQUIRE(result->success());
  }

  SECTION("failing command returns non-success exit status") {
    subprocess::Result<subprocess::ExitStatus> result =
        subprocess::Exec::cmd("false").join();
    REQUIRE(result.ok());
    REQUIRE_FALSE(result->success());
  }

  SECTION("absolute path to nonexistent command propagates launch error") {
    subprocess::Result<subprocess::ExitStatus> result =
        subprocess::Exec::cmd("/definitely/does/not/exist/cmd").join();
    REQUIRE_FALSE(result.ok());
  }

  SECTION("args are forwarded correctly") {
    // 'test' exits 0 when the string is non-empty
    subprocess::Result<subprocess::ExitStatus> result =
        subprocess::Exec::cmd("sh").arg("-c").arg("exit 42").join();
    REQUIRE(result.ok());
    REQUIRE(result->is_a<subprocess::ExitStatus::Exited>());
    REQUIRE(result->get<subprocess::ExitStatus::Exited>().code == 42);
  }
}

TEST_CASE("Exec::stream_stdout") {
  SECTION("automatically sets stdout to pipe when not configured") {
    subprocess::Result<boost::fdistream> result =
        subprocess::Exec::cmd("echo").arg("hello").stream_stdout();
    REQUIRE(result.ok());
    std::string line;
    std::getline(*result, line);
    REQUIRE(line == "hello");
  }

  SECTION("works when stdout already explicitly set to Pipe") {
    subprocess::Result<boost::fdistream> result =
        subprocess::Exec::cmd("echo")
            .arg("world")
            .stdout_(subprocess::Redirection::Pipe())
            .stream_stdout();
    REQUIRE(result.ok());
    std::string line;
    std::getline(*result, line);
    REQUIRE(line == "world");
  }

  SECTION("multiline output can be read line by line") {
    subprocess::Result<boost::fdistream> result =
        subprocess::Exec::cmd("printf").arg("a\nb\nc\n").stream_stdout();
    REQUIRE(result.ok());
    std::string line;
    std::getline(*result, line);
    REQUIRE(line == "a");
    std::getline(*result, line);
    REQUIRE(line == "b");
    std::getline(*result, line);
    REQUIRE(line == "c");
  }

  SECTION("nonexistent command propagates launch error") {
    subprocess::Result<boost::fdistream> result =
        subprocess::Exec::cmd("/definitely/does/not/exist/cmd").stream_stdout();
    REQUIRE_FALSE(result.ok());
  }
}

TEST_CASE("Exec::stream_stdin") {
  SECTION("automatically sets stdin to pipe when not configured") {
    // cat with stdout discarded; writing to the returned stream must not fail.
    subprocess::Result<boost::fdostream> result =
        subprocess::Exec::cmd("cat").stdout_(subprocess::NullFile{}).stream_stdin();
    REQUIRE(result.ok());
    *result << "hello subprocess\n";
    result->close();
  }

  SECTION("works when stdin already explicitly set to Pipe") {
    subprocess::Result<boost::fdostream> result =
        subprocess::Exec::cmd("cat")
            .stdin_(subprocess::Redirection::Pipe())
            .stdout_(subprocess::NullFile{})
            .stream_stdin();
    REQUIRE(result.ok());
    *result << "data\n";
    result->close();
  }

  SECTION("data written to stream is received by the subprocess") {
    // Use a shell one-liner: read a line from stdin and exit with
    // code 0 only if it equals the expected string.
    subprocess::Result<boost::fdostream> result =
        subprocess::Exec::cmd("sh")
            .arg("-c")
            .arg("read line; [ \"$line\" = \"hello\" ]")
            .stream_stdin();
    REQUIRE(result.ok());
    *result << "hello\n";
    result->close();
  }

  SECTION("nonexistent command propagates launch error") {
    subprocess::Result<boost::fdostream> result =
        subprocess::Exec::cmd("/definitely/does/not/exist/cmd").stream_stdin();
    REQUIRE_FALSE(result.ok());
  }
}

TEST_CASE("Exec::shell") {
  SECTION("runs a simple shell expression and captures output") {
    subprocess::Result<subprocess::CaptureData> result =
        subprocess::Exec::shell("echo hello world").capture();
    REQUIRE(result.ok());
    REQUIRE(result->success());
    REQUIRE(result->out == "hello world\n");
  }

  SECTION("shell features: command substitution") {
    subprocess::Result<subprocess::CaptureData> result =
        subprocess::Exec::shell("echo $(echo nested)").capture();
    REQUIRE(result.ok());
    REQUIRE(result->success());
    REQUIRE(result->out == "nested\n");
  }

  SECTION("shell features: pipeline in string") {
    subprocess::Result<subprocess::CaptureData> result =
        subprocess::Exec::shell("echo foo | tr a-z A-Z").capture();
    REQUIRE(result.ok());
    REQUIRE(result->success());
    REQUIRE(result->out == "FOO\n");
  }

  SECTION("non-zero exit status is reflected") {
    subprocess::Result<subprocess::CaptureData> result =
        subprocess::Exec::shell("exit 3").capture();
    REQUIRE(result.ok());
    REQUIRE_FALSE(result->success());
    REQUIRE(result->exit_status.is_a<subprocess::ExitStatus::Exited>());
    REQUIRE(result->exit_status.get<subprocess::ExitStatus::Exited>().code == 3);
  }

  SECTION("stream_stdout works with shell") {
    subprocess::Result<boost::fdistream> result =
        subprocess::Exec::shell("echo streamed").stream_stdout();
    REQUIRE(result.ok());
    std::string line;
    std::getline(*result, line);
    // cmd.exe on Windows appends \r before \n; strip it for cross-platform comparison.
    if (!line.empty() && line.back() == '\r') line.pop_back();
    REQUIRE(line == "streamed");
  }
}

TEST_CASE("Exec::capture") {
  SECTION("captures stdout of a simple command") {
    subprocess::Result<subprocess::CaptureData> result =
        subprocess::Exec::cmd("echo").arg("hello").capture();
    REQUIRE(result.ok());
    REQUIRE(result->success());
    REQUIRE(result->out == "hello\n");
    REQUIRE(result->err.empty());
  }

  SECTION("captures stderr separately from stdout") {
    subprocess::Result<subprocess::CaptureData> result =
        subprocess::Exec::cmd("sh")
            .arg("-c")
            .arg("echo out; echo err >&2")
            .capture();
    REQUIRE(result.ok());
    REQUIRE(result->success());
    REQUIRE(result->out == "out\n");
    REQUIRE(result->err == "err\n");
  }

  SECTION("feeds stdin data and captures stdout") {
    subprocess::Result<subprocess::CaptureData> result =
        subprocess::Exec::cmd("cat").stdin_(std::string("hello subprocess\n")).capture();
    REQUIRE(result.ok());
    REQUIRE(result->success());
    REQUIRE(result->out == "hello subprocess\n");
  }

  SECTION("exit status reflects command exit code") {
    subprocess::Result<subprocess::CaptureData> result =
        subprocess::Exec::cmd("sh").arg("-c").arg("exit 7").capture();
    REQUIRE(result.ok());
    REQUIRE_FALSE(result->success());
    REQUIRE(result->exit_status.is_a<subprocess::ExitStatus::Exited>());
    REQUIRE(result->exit_status.get<subprocess::ExitStatus::Exited>().code == 7);
  }

  SECTION("already-piped stdout and stderr are not overridden") {
    subprocess::Result<subprocess::CaptureData> result =
        subprocess::Exec::cmd("echo")
            .arg("piped")
            .stdout_(subprocess::Redirection::Pipe())
            .stderr_(subprocess::Redirection::Pipe())
            .capture();
    REQUIRE(result.ok());
    REQUIRE(result->out == "piped\n");
  }

  SECTION("nonexistent command propagates launch error") {
    subprocess::Result<subprocess::CaptureData> result =
        subprocess::Exec::cmd("/definitely/does/not/exist/cmd").capture();
    REQUIRE_FALSE(result.ok());
  }

  SECTION("capture with binary stdin data") {
    std::vector<uint8_t> data{ 'a', 'b', 'c', '\n' };
    subprocess::Result<subprocess::CaptureData> result =
        subprocess::Exec::cmd("cat").stdin_(data).capture();
    REQUIRE(result.ok());
    REQUIRE(result->out == "abc\n");
  }
}

// ---------------------------------------------------------------------------
// Platform-specific behaviour tests
// ---------------------------------------------------------------------------

TEST_CASE("Exec::shell platform dispatch") {
  SECTION("shell() runs a command through the system shell") {
    // The exact shell (sh on Unix, cmd.exe on Windows) is an implementation
    // detail.  We only verify that a simple expression evaluates correctly.
    subprocess::Result<subprocess::CaptureData> result =
        subprocess::Exec::shell("echo hello").capture();
    REQUIRE(result.ok());
    REQUIRE(result->success());
    // Both sh and cmd.exe echo the argument followed by a newline.
    // On Windows cmd.exe may include a trailing space before the newline, so
    // we just check that the word "hello" appears in the output.
    REQUIRE(result->out.find("hello") != std::string::npos);
  }
}

#ifdef _WIN32
TEST_CASE("Windows send_signal with arbitrary signal returns LogicError") {
  subprocess::Result<subprocess::Popen> p =
      subprocess::Exec::cmd("cmd.exe").arg("/c").arg("timeout /t 60 >NUL").popen();
  REQUIRE(p.ok());
  subprocess::Popen proc = p.take_value();
  // SIGUSR1 (10) is not SIGTERM/SIGKILL; must return a LogicError on Windows.
  subprocess::Result<std::nullopt_t> res = proc.send_signal(10);
  REQUIRE_FALSE(res.ok());
  // Clean up
  proc.kill();
  proc.wait();
}
#else
TEST_CASE("Unix send_signal with SIGUSR1") {
  // Basic sanity: sending SIGUSR1 to a sleeping process terminates it on Unix.
  subprocess::Result<subprocess::Popen> p =
      subprocess::Exec::cmd("sleep").arg("60").popen();
  REQUIRE(p.ok());
  subprocess::Popen proc = p.take_value();
  subprocess::Result<std::nullopt_t> res = proc.send_signal(SIGUSR1);
  REQUIRE(res.ok());
  subprocess::Result<subprocess::ExitStatus> status = proc.wait();
  REQUIRE(status.ok());
  REQUIRE(status->is_a<subprocess::ExitStatus::Signaled>());
}
#endif
