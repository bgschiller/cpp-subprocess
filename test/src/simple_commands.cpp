#include <catch2/catch.hpp>
#include <fcntl.h>
#include <optional>
#include <signal.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

#include "subprocess/Popen.hpp"
#include "subprocess/Redirection.hpp"
#include "subprocess/posix.hpp"

using namespace subprocess;

TEST_CASE("echo time") {
  SECTION("to stdout") {
    auto echoR = Popen::create({"echo", "yolo"}, PopenConfig{});
    REQUIRE(echoR.ok());
    auto echo = echoR.take_value();
    auto exitR = echo.wait();
    REQUIRE(exitR.ok());
    auto exit = exitR.take_value();
    REQUIRE(exit.success());
  }

  SECTION("pipe output") {
    PopenConfig config;
    config.stdout = Redirection::Pipe();
    auto echoR = Popen::create({"echo", "yolo"}, config);
    REQUIRE(echoR.ok());
    auto echo = echoR.take_value();
    std::string buf;
    REQUIRE_FALSE(echo.std_out->eof());
    std::getline(*echo.std_out, buf);
    REQUIRE(buf == "yolo");
    auto exitR = echo.wait();
    REQUIRE(exitR.ok());
    auto exit = exitR.take_value();
    REQUIRE(exit.success());
  }

  SECTION("pipe input and output") {
    PopenConfig config;
    config.stdin = Redirection::Pipe();
    config.stdout = Redirection::Pipe();
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
    FILE* fruits = fopen("fruits.tmp", "w");
    fprintf(fruits, "apple\nbanana\npineapple\nlemon\n");
    fclose(fruits);

    PopenConfig config;
    SECTION("file descriptor") {
      int fruitsFd = open("fruits.tmp", O_RDONLY);
      config.stdin = std::move(Redirection::FileDescriptor(fruitsFd));
    }
    SECTION("shorthand") {
      config.stdin = Redirection::Read("fruits.tmp").or_throw();
    }
    config.stdout = Redirection::Pipe();
    auto grep = Popen::create({"grep", "apple"}, config).or_throw();
    auto exit = grep.wait().or_throw();
    REQUIRE(exit.success());

    REQUIRE(grep.std_out->slurp() == "apple\npineapple\n");
  }

  SECTION("Two process pipeline") {
    FILE* veggies = fopen("veggies.tmp", "w");
    fprintf(veggies, "brussels sprouts\nkale\ncarrots\nbroccoli\ncauliflower\neggplant\nspinach\n");
    fclose(veggies);

    auto catToGrep = subprocess::pipe().or_throw();
    PopenConfig catCfg;
    catCfg.stdout = Redirection::FileDescriptor(std::get<1>(catToGrep));
    PopenConfig grepCfg;
    grepCfg.stdin = Redirection::FileDescriptor(std::get<0>(catToGrep));
    grepCfg.stdout = Redirection::Pipe();

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
}

TEST_CASE("Popen destructor") {
  SECTION("reaps child that was never explicitly waited on") {
    // The child runs to completion and the destructor must call wait(),
    // preventing it from becoming a zombie.
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
  }

  SECTION("closes stdin pipe so a blocking child exits") {
    // `cat` with a Pipe stdin blocks until EOF.  The destructor must close
    // std_in (sending EOF) and then wait — without hanging.
    PopenConfig cfg;
    cfg.stdin = Redirection::Pipe();
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
  }
}

TEST_CASE("signal methods") {
  SECTION("kill() terminates a running process with SIGKILL") {
    auto p = subprocess::Popen::create({"sleep", "60"}, subprocess::PopenConfig{}).or_throw();
    REQUIRE(p.pid().has_value());

    auto sig_result = p.kill();
    REQUIRE(sig_result.ok());

    auto exit = p.wait().or_throw();
    REQUIRE_FALSE(exit.success());
    REQUIRE(exit.is_a<subprocess::ExitStatus::Signaled>());
    REQUIRE(exit.get<subprocess::ExitStatus::Signaled>().signal == SIGKILL);
  }

  SECTION("terminate() sends SIGTERM to a running process") {
    // Use a process that explicitly handles SIGTERM and exits non-zero
    // so we can distinguish a signal death from a clean exit.
    auto p = subprocess::Popen::create({"sleep", "60"}, subprocess::PopenConfig{}).or_throw();
    REQUIRE(p.pid().has_value());

    auto sig_result = p.terminate();
    REQUIRE(sig_result.ok());

    auto exit = p.wait().or_throw();
    REQUIRE_FALSE(exit.success());
    REQUIRE(exit.is_a<subprocess::ExitStatus::Signaled>());
    REQUIRE(exit.get<subprocess::ExitStatus::Signaled>().signal == SIGTERM);
  }

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

  SECTION("send_signal() on a finished process returns LogicError") {
    auto p = subprocess::Popen::create({"true"}, subprocess::PopenConfig{}).or_throw();
    p.wait().or_throw();  // process has now finished

    auto sig_result = p.send_signal(SIGTERM);
    REQUIRE_FALSE(sig_result.ok());
    auto err = sig_result.take_error();
    REQUIRE(err.kind == subprocess::PopenError::LogicError);
  }

  SECTION("kill() on a finished process returns LogicError") {
    auto p = subprocess::Popen::create({"true"}, subprocess::PopenConfig{}).or_throw();
    p.wait().or_throw();

    auto sig_result = p.kill();
    REQUIRE_FALSE(sig_result.ok());
    REQUIRE(sig_result.take_error().kind == subprocess::PopenError::LogicError);
  }

  SECTION("terminate() on a finished process returns LogicError") {
    auto p = subprocess::Popen::create({"true"}, subprocess::PopenConfig{}).or_throw();
    p.wait().or_throw();

    auto sig_result = p.terminate();
    REQUIRE_FALSE(sig_result.ok());
    REQUIRE(sig_result.take_error().kind == subprocess::PopenError::LogicError);
  }
}

TEST_CASE("exit status decoding") {
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
    auto p = Popen::create({"/bin/sh", "-c", "exit 42"}, PopenConfig{}).or_throw();
    auto exit = p.wait().or_throw();
    REQUIRE_FALSE(exit.success());
    REQUIRE(exit.is_a<ExitStatus::Exited>());
    REQUIRE(exit.get<ExitStatus::Exited>().code == 42);
  }

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
}
