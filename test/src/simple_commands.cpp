#ifdef _WIN32
#  define _CRT_SECURE_NO_WARNINGS
#  include <fcntl.h>
#  include <io.h>
#  include <signal.h>
#else
#  include <fcntl.h>
#  include <signal.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif
#include <catch2/catch.hpp>
#include <optional>
#include <string>
#include <vector>

#include "subprocess/Popen.hpp"
#include "subprocess/Redirection.hpp"
#include "subprocess/posix.hpp"

using namespace subprocess;

// On Windows, /bin/sh does not exist but Git for Windows ships sh.exe on PATH.
#ifdef _WIN32
static constexpr const char* kSh = "sh";
#else
static constexpr const char* kSh = "/bin/sh";
#endif

#ifdef _WIN32
static void diag(const char* msg) { std::cerr << "[diag] " << msg << std::endl; }
#else
static void diag(const char*) {}
#endif

TEST_CASE("echo time") {
  SECTION("to stdout") {
    diag("echo-time: to-stdout");
    auto echoR = Popen::create({"echo", "yolo"}, PopenConfig{});
    REQUIRE(echoR.ok());
    auto echo = echoR.take_value();
    auto exitR = echo.wait();
    REQUIRE(exitR.ok());
    auto exit = exitR.take_value();
    REQUIRE(exit.success());
  }

  SECTION("pipe output") {
    diag("echo-time: pipe-output-start");
    PopenConfig config;
    config.stdout_ = Redirection::Pipe();
    auto echoR = Popen::create({"echo", "yolo"}, config);
    REQUIRE(echoR.ok());
    diag("echo-time: pipe-output-created");
    auto echo = echoR.take_value();
    std::string buf;
    REQUIRE_FALSE(echo.std_out->eof());
    std::getline(*echo.std_out, buf);
    // Windows cmd.exe echo appends \r before \n; strip it.
    if (!buf.empty() && buf.back() == '\r') buf.pop_back();
    REQUIRE(buf == "yolo");
    auto exitR = echo.wait();
    REQUIRE(exitR.ok());
    auto exit = exitR.take_value();
    REQUIRE(exit.success());
  }

  SECTION("pipe input and output") {
    diag("echo-time: pipe-io-start");
    PopenConfig config;
    config.stdin_ = Redirection::Pipe();
    config.stdout_ = Redirection::Pipe();
    auto grepR = Popen::create({"grep", "apple"}, config);
    REQUIRE(grepR.ok());
    auto grep = grepR.take_value();
    REQUIRE(grep.poll() == std::nullopt);

    REQUIRE(grep.std_in.has_value());
    *grep.std_in << "apple\n"
      << "banana\n"
      << "pineapple\n"
      << "lemon\n";

    grep.std_in->close();

    auto exitR = grep.wait();

    REQUIRE(exitR.ok());

    auto exit = exitR.take_value();
    REQUIRE(exit.success());

    REQUIRE(grep.std_out->slurp() == "apple\npineapple\n");
  }

  SECTION("input from file") {
    diag("echo-time: input-from-file-start");
    FILE* fruits = fopen("fruits.tmp", "w");
    fprintf(fruits, "apple\nbanana\npineapple\nlemon\n");
    fclose(fruits);

    PopenConfig config;
#ifndef _WIN32
    SECTION("file descriptor") {
      int fruitsFd = open("fruits.tmp", O_RDONLY);
      config.stdin_ = std::move(Redirection::FileDescriptor(fruitsFd));
    }
#endif
    SECTION("shorthand") {
      config.stdin_ = Redirection::Read("fruits.tmp").or_throw();
    }
    config.stdout_ = Redirection::Pipe();
    auto grep = Popen::create({"grep", "apple"}, config).or_throw();
    auto exit = grep.wait().or_throw();
    REQUIRE(exit.success());

    REQUIRE(grep.std_out->slurp() == "apple\npineapple\n");
  }

#ifndef _WIN32
  SECTION("Two process pipeline") {
    FILE* veggies = fopen("veggies.tmp", "w");
    fprintf(veggies, "brussels sprouts\nkale\ncarrots\nbroccoli\ncauliflower\neggplant\nspinach\n");
    fclose(veggies);

    auto catToGrep = subprocess::pipe().or_throw();
    PopenConfig catCfg;
    catCfg.stdout_ = Redirection::FileDescriptor(std::get<1>(catToGrep));
    PopenConfig grepCfg;
    grepCfg.stdin_ = Redirection::FileDescriptor(std::get<0>(catToGrep));
    grepCfg.stdout_ = Redirection::Pipe();

    auto cat = Popen::create({"cat", "veggies.tmp"}, catCfg).or_throw();
    auto grep = Popen::create({"grep", "sp"}, grepCfg).or_throw();
    ::close(std::get<0>(catToGrep));
    ::close(std::get<1>(catToGrep));
    auto cExit = cat.wait().or_throw();
    auto gExit = grep.wait().or_throw();
    REQUIRE(gExit.success());
    REQUIRE(cExit.success());
    REQUIRE(grep.std_out->slurp() == "brussels sprouts\nspinach\n");
  }
#endif
}

TEST_CASE("Popen destructor") {
  diag("dtor: start");
  SECTION("reaps child that was never explicitly waited on") {
    // The child runs to completion and the destructor must call wait(),
    // preventing it from becoming a zombie.
#ifndef _WIN32
    pid_t child_pid;
    {
      auto p = Popen::create({"true"}, PopenConfig{}).or_throw();
      child_pid = p.pid().value();
      // p goes out of scope here — destructor should reap the child.
    }
    // After the destructor ran, waitpid must fail with ECHILD,
    // confirming there is no zombie.
    int status = 0;
    pid_t ret = ::waitpid(child_pid, &status, 0);
    REQUIRE(ret == -1);
    REQUIRE(errno == ECHILD);
#else
    // On Windows there is no waitpid; we just verify the destructor doesn't
    // block or crash.
    { auto p = Popen::create({"cmd.exe", "/c", "exit 0"}, PopenConfig{}).or_throw(); }
    REQUIRE(true);
#endif
  }

  SECTION("closes stdin pipe so a blocking child exits") {
    // `cat` with a Pipe stdin blocks until EOF.  The destructor must close
    // std_in (sending EOF) and then wait — without hanging.
    PopenConfig cfg;
    cfg.stdin_ = Redirection::Pipe();
    {
      auto p = Popen::create({"cat"}, cfg).or_throw();
      // No data written, no explicit close.  Destructor handles everything.
    }
    // If execution reaches here the test passes (no hang).
    REQUIRE(true);
  }

  SECTION("does nothing for a detached process") {
    // A detached Popen must not be waited on, so we should be able to
    // reap the child ourselves after the destructor returns.
    PopenConfig cfg;
    cfg.detached = true;
#ifndef _WIN32
    pid_t child_pid;
    {
      auto p = Popen::create({"true"}, cfg).or_throw();
      child_pid = p.pid().value();
      // Destructor must NOT call waitpid.
    }
    // Reap the child ourselves — this must succeed.
    int status = 0;
    pid_t ret = ::waitpid(child_pid, &status, 0);
    REQUIRE(ret == child_pid);
    REQUIRE(WIFEXITED(status));
    REQUIRE(WEXITSTATUS(status) == 0);
#else
    // On Windows, just verify the detached Popen creates and destructs cleanly.
    auto p = Popen::create({"cmd.exe", "/c", "exit 0"}, cfg).or_throw();
    subprocess::pid_type pid = p.pid().value();
    (void)pid;
    // The destructor should not wait on a detached process.
#endif
  }
}

TEST_CASE("signal methods") {
  diag("signal: start");
  SECTION("kill() terminates a running process") {
#ifndef _WIN32
    auto p = subprocess::Popen::create({"sleep", "60"}, subprocess::PopenConfig{}).or_throw();
    REQUIRE(p.pid().has_value());

    auto sig_result = p.kill();
    REQUIRE(sig_result.ok());

    auto exit = p.wait().or_throw();
    REQUIRE_FALSE(exit.success());
    REQUIRE(exit.is_a<subprocess::ExitStatus::Signaled>());
    REQUIRE(exit.get<subprocess::ExitStatus::Signaled>().signal == SIGKILL);
#else
    auto p = subprocess::Popen::create({"cmd.exe", "/c", "timeout /t 60 >nul"}, subprocess::PopenConfig{}).or_throw();
    REQUIRE(p.pid().has_value());

    auto sig_result = p.kill();
    REQUIRE(sig_result.ok());

    auto exit = p.wait().or_throw();
    REQUIRE_FALSE(exit.success());
#endif
  }

  SECTION("terminate() terminates a running process") {
#ifndef _WIN32
    auto p = subprocess::Popen::create({"sleep", "60"}, subprocess::PopenConfig{}).or_throw();
    REQUIRE(p.pid().has_value());

    auto sig_result = p.terminate();
    REQUIRE(sig_result.ok());

    auto exit = p.wait().or_throw();
    REQUIRE_FALSE(exit.success());
    REQUIRE(exit.is_a<subprocess::ExitStatus::Signaled>());
    REQUIRE(exit.get<subprocess::ExitStatus::Signaled>().signal == SIGTERM);
#else
    auto p = subprocess::Popen::create({"cmd.exe", "/c", "timeout /t 60 >nul"}, subprocess::PopenConfig{}).or_throw();
    REQUIRE(p.pid().has_value());

    auto sig_result = p.terminate();
    REQUIRE(sig_result.ok());

    auto exit = p.wait().or_throw();
    REQUIRE_FALSE(exit.success());
#endif
  }

#ifndef _WIN32
  SECTION("send_signal() with an arbitrary signal") {
    auto p = subprocess::Popen::create({"sleep", "60"}, subprocess::PopenConfig{}).or_throw();
    REQUIRE(p.pid().has_value());

    auto sig_result = p.send_signal(SIGUSR1);
    REQUIRE(sig_result.ok());

    // sleep exits when it receives SIGUSR1
    auto exit = p.wait().or_throw();
    REQUIRE(exit.is_a<subprocess::ExitStatus::Signaled>());
    REQUIRE(exit.get<subprocess::ExitStatus::Signaled>().signal == SIGUSR1);
  }
#endif

  SECTION("send_signal() on a finished process returns LogicError") {
#ifndef _WIN32
    auto p = subprocess::Popen::create({"true"}, subprocess::PopenConfig{}).or_throw();
#else
    auto p = subprocess::Popen::create({"cmd.exe", "/c", "exit 0"}, subprocess::PopenConfig{}).or_throw();
#endif
    p.wait().or_throw();  // process has now finished

    auto sig_result = p.send_signal(SIGTERM);
    REQUIRE_FALSE(sig_result.ok());
    auto err = sig_result.take_error();
    REQUIRE(err.kind == subprocess::PopenError::LogicError);
  }

  SECTION("kill() on a finished process returns LogicError") {
#ifndef _WIN32
    auto p = subprocess::Popen::create({"true"}, subprocess::PopenConfig{}).or_throw();
#else
    auto p = subprocess::Popen::create({"cmd.exe", "/c", "exit 0"}, subprocess::PopenConfig{}).or_throw();
#endif
    p.wait().or_throw();

    auto sig_result = p.kill();
    REQUIRE_FALSE(sig_result.ok());
    REQUIRE(sig_result.take_error().kind == subprocess::PopenError::LogicError);
  }

  SECTION("terminate() on a finished process returns LogicError") {
#ifndef _WIN32
    auto p = subprocess::Popen::create({"true"}, subprocess::PopenConfig{}).or_throw();
#else
    auto p = subprocess::Popen::create({"cmd.exe", "/c", "exit 0"}, subprocess::PopenConfig{}).or_throw();
#endif
    p.wait().or_throw();

    auto sig_result = p.terminate();
    REQUIRE_FALSE(sig_result.ok());
    REQUIRE(sig_result.take_error().kind == subprocess::PopenError::LogicError);
  }
}

TEST_CASE("Popen::communicate") {
  diag("communicate: start");
  SECTION("captures stdout with no stdin") {
    subprocess::PopenConfig cfg;
    cfg.stdout_ = subprocess::Redirection::Pipe();
    auto p = subprocess::Popen::create({"echo", "hello"}, cfg).or_throw();
    auto result = p.communicate().or_throw();
    REQUIRE(result.out == "hello\n");
    REQUIRE(result.err.empty());
    REQUIRE(result.success());
  }

  SECTION("sends stdin input and captures stdout") {
    subprocess::PopenConfig cfg;
    cfg.stdin_ = subprocess::Redirection::Pipe();
    cfg.stdout_ = subprocess::Redirection::Pipe();
    auto p = subprocess::Popen::create({"cat"}, cfg).or_throw();
    auto result = p.communicate("hello world").or_throw();
    REQUIRE(result.out == "hello world");
    REQUIRE(result.success());
  }

  SECTION("captures both stdout and stderr") {
    subprocess::PopenConfig cfg;
    cfg.stdout_ = subprocess::Redirection::Pipe();
    cfg.stderr_ = subprocess::Redirection::Pipe();
    auto p =
        subprocess::Popen::create(
            {kSh, "-c", "echo out; echo err >&2"}, cfg)
            .or_throw();
    auto result = p.communicate().or_throw();
    REQUIRE(result.out == "out\n");
    REQUIRE(result.err == "err\n");
    REQUIRE(result.success());
  }

  SECTION("large input does not deadlock") {
    // 1 MB of data — well above the typical pipe buffer size (~64 KB).
    subprocess::PopenConfig cfg;
    cfg.stdin_ = subprocess::Redirection::Pipe();
    cfg.stdout_ = subprocess::Redirection::Pipe();
    auto p = subprocess::Popen::create({"cat"}, cfg).or_throw();
    std::string big_input(1024 * 1024, 'x');
    auto result = p.communicate(big_input).or_throw();
    REQUIRE(result.out == big_input);
    REQUIRE(result.success());
  }

  SECTION("communicate_bytes with binary data") {
    subprocess::PopenConfig cfg;
    cfg.stdin_ = subprocess::Redirection::Pipe();
    cfg.stdout_ = subprocess::Redirection::Pipe();
    auto p = subprocess::Popen::create({"cat"}, cfg).or_throw();
    std::vector<uint8_t> data = { 0x00, 0x01, 0xFF, 0x7F };
    auto result = p.communicate_bytes(data).or_throw();
    REQUIRE(result.out.size() == 4);
    REQUIRE(static_cast<uint8_t>(result.out[0]) == 0x00);
    REQUIRE(static_cast<uint8_t>(result.out[1]) == 0x01);
    REQUIRE(static_cast<uint8_t>(result.out[2]) == 0xFF);
    REQUIRE(static_cast<uint8_t>(result.out[3]) == 0x7F);
    REQUIRE(result.success());
  }

  SECTION("input without stdin pipe returns LogicError") {
    subprocess::PopenConfig cfg;
    // stdin is NOT a pipe
    auto p = subprocess::Popen::create({"true"}, cfg).or_throw();
    auto result = p.communicate("some input");
    REQUIRE_FALSE(result.ok());
    REQUIRE(result.take_error().kind == subprocess::PopenError::LogicError);
    // Process still needs to be waited on after the failed communicate().
    p.wait();
  }

  SECTION("communicate with no pipes just waits") {
    subprocess::PopenConfig cfg;
    auto p = subprocess::Popen::create({"true"}, cfg).or_throw();
    auto result = p.communicate().or_throw();
    REQUIRE(result.out.empty());
    REQUIRE(result.err.empty());
    REQUIRE(result.success());
  }

  SECTION("communicate propagates non-zero exit status") {
    subprocess::PopenConfig cfg;
    cfg.stdout_ = subprocess::Redirection::Pipe();
    auto p =
        subprocess::Popen::create({kSh, "-c", "exit 42"}, cfg).or_throw();
    auto result = p.communicate().or_throw();
    REQUIRE_FALSE(result.success());
    REQUIRE(result.exit_status.is_a<subprocess::ExitStatus::Exited>());
    REQUIRE(result.exit_status.get<subprocess::ExitStatus::Exited>().code == 42);
  }
}

TEST_CASE("exit status decoding") {
  diag("exit-status: start");
  SECTION("exit code 0 is success") {
    auto p = Popen::create({"true"}, PopenConfig{}).or_throw();
    auto exit = p.wait().or_throw();
    REQUIRE(exit.success());
    REQUIRE(exit.is_a<ExitStatus::Exited>());
    REQUIRE(exit.get<ExitStatus::Exited>().code == 0);
  }

  SECTION("non-zero exit code is decoded correctly") {
    // Without the fix, exit(42) produces raw status 10752 (42<<8)
    // and would be stored as Exited{10752} rather than Exited{42}.
    auto p = Popen::create({kSh, "-c", "exit 42"}, PopenConfig{}).or_throw();
    auto exit = p.wait().or_throw();
    REQUIRE_FALSE(exit.success());
    REQUIRE(exit.is_a<ExitStatus::Exited>());
    REQUIRE(exit.get<ExitStatus::Exited>().code == 42);
  }

#ifndef _WIN32
  SECTION("signal termination is decoded correctly") {
    auto p = Popen::create({"sleep", "60"}, PopenConfig{}).or_throw();
    auto maybe_pid = p.pid();
    REQUIRE(maybe_pid.has_value());
    ::kill(*maybe_pid, SIGKILL);
    auto exit = p.wait().or_throw();
    REQUIRE_FALSE(exit.success());
    REQUIRE(exit.is_a<ExitStatus::Signaled>());
    REQUIRE(exit.get<ExitStatus::Signaled>().signal == SIGKILL);
  }
#endif
}
