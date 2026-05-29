// windows_diagnostic_test.cpp
// Isolated probes for crash sites that are hard to diagnose on Windows CI.
// Each SECTION is self-contained and prints a "reached" marker to stderr
// (unbuffered) before and after the interesting operation.  When the test
// binary crashes, the last "reached" line tells us exactly where.
//
// This file is compiled on all platforms but only registers tests on Windows.

#ifdef _WIN32

#include <catch2/catch.hpp>
#include <cstdio>
#include <iostream>
#include <string>

#include "subprocess/Exec.hpp"
#include "subprocess/Popen.hpp"
#include "subprocess/Redirection.hpp"

// Helper: flush stderr with a label so it survives a hard crash.
static void probe(const char* label) {
  std::cerr << "[diag] " << label << std::endl;  // endl flushes
}

TEST_CASE("Windows diagnostic: pipe read with CRLF") {
  SECTION("echo yolo into pipe - read line") {
    probe("pipe-read: start");
    subprocess::PopenConfig cfg;
    cfg.stdout_ = subprocess::Redirection::Pipe();
    auto p = subprocess::Popen::create({"echo", "yolo"}, cfg).or_throw();
    probe("pipe-read: process created");
    std::string buf;
    std::getline(*p.std_out, buf);
    probe(("pipe-read: getline returned buf=" + buf).c_str());
    // Strip \r so the test passes on both Unix and Windows.
    if (!buf.empty() && buf.back() == '\r') buf.pop_back();
    REQUIRE(buf == "yolo");
    p.wait().or_throw();
    probe("pipe-read: done");
  }

  SECTION("grep through pipe - slurp with CRLF") {
    probe("grep-slurp: start");
    subprocess::PopenConfig cfg;
    cfg.stdin_ = subprocess::Redirection::Pipe();
    cfg.stdout_ = subprocess::Redirection::Pipe();
    auto p = subprocess::Popen::create({"grep", "apple"}, cfg).or_throw();
    probe("grep-slurp: process created");
    *p.std_in << "apple\nbanana\npineapple\nlemon\n";
    p.std_in->close();
    probe("grep-slurp: stdin closed");
    std::string out = p.std_out->slurp();
    probe(("grep-slurp: slurp returned len=" + std::to_string(out.size())).c_str());
    p.wait().or_throw();
    probe("grep-slurp: done");
  }
}

TEST_CASE("Windows diagnostic: Popen destructor with cat stdin pipe") {
  probe("dtor-cat: start");
  {
    subprocess::PopenConfig cfg;
    cfg.stdin_ = subprocess::Redirection::Pipe();
    auto p = subprocess::Popen::create({"cat"}, cfg).or_throw();
    probe("dtor-cat: process created, about to destruct");
  }
  probe("dtor-cat: destructor returned");
  REQUIRE(true);
}

TEST_CASE("Windows diagnostic: kill running process") {
  probe("kill: start");
  auto p = subprocess::Popen::create(
               {"cmd.exe", "/c", "timeout /t 60 >nul"},
               subprocess::PopenConfig{})
               .or_throw();
  probe("kill: process created");
  auto sig_result = p.kill();
  probe(("kill: kill() returned ok=" + std::to_string(sig_result.ok())).c_str());
  REQUIRE(sig_result.ok());
  auto exit = p.wait().or_throw();
  probe(("kill: wait returned success=" + std::to_string(exit.success())).c_str());
  REQUIRE_FALSE(exit.success());
  probe("kill: done");
}

TEST_CASE("Windows diagnostic: communicate large input") {
  probe("communicate-large: start");
  subprocess::PopenConfig cfg;
  cfg.stdin_ = subprocess::Redirection::Pipe();
  cfg.stdout_ = subprocess::Redirection::Pipe();
  auto p = subprocess::Popen::create({"cat"}, cfg).or_throw();
  probe("communicate-large: process created");
  std::string big_input(64 * 1024, 'x');  // 64 KB (not 1 MB — avoid deadlock probe)
  auto result = p.communicate(big_input).or_throw();
  probe(("communicate-large: result.out.size()=" + std::to_string(result.out.size())).c_str());
  REQUIRE(result.out == big_input);
  REQUIRE(result.success());
  probe("communicate-large: done");
}

TEST_CASE("Windows diagnostic: NullFile stdout") {
  probe("nullfile: start");
  auto result =
      subprocess::Exec::cmd("cat").stdout_(subprocess::NullFile{}).stream_stdin();
  probe(("nullfile: stream_stdin returned ok=" + std::to_string(result.ok())).c_str());
  REQUIRE(result.ok());
  *result << "hello\n";
  result->close();
  probe("nullfile: done");
}

#endif  // _WIN32
