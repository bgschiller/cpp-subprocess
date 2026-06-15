#ifdef _WIN32
#  define _CRT_SECURE_NO_WARNINGS
#endif
#include <catch2/catch.hpp>

#include "subprocess/Exec.hpp"
#include "subprocess/operators.hpp"
#include "subprocess/Redirection.hpp"

#ifdef _WIN32
#else
#endif

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
                    .set_stdin(subprocess::NullFile{})
                    .join()
                    .or_throw();
  REQUIRE_FALSE(status.success());
}

TEST_CASE("Pipeline: stdout(NullFile) discards last stage output") {
  auto status =
      (subprocess::Exec::cmd("echo").arg("hello") | subprocess::Exec::cmd("cat"))
          .set_stdout(subprocess::NullFile{})
          .join()
          .or_throw();
  REQUIRE(status.success());
}

// ---------------------------------------------------------------------------
// File redirection operators (< > >>)
// ---------------------------------------------------------------------------

TEST_CASE("Exec operator< redirects stdin from file") {
  // Write a test file
  {
    FILE* f = fopen("redirect_test_in.txt", "w");
    fprintf(f, "hello from file\n");
    fclose(f);
  }

  // cat reads from the file and writes to stdout (captured)
  auto data =
      (subprocess::Exec::cmd("cat") < "redirect_test_in.txt").capture().or_throw();
  REQUIRE(data.out == "hello from file\n");
  REQUIRE(data.success());

  remove("redirect_test_in.txt");
}

TEST_CASE("Exec operator> redirects stdout to file (overwrite)") {
  {
    auto status =
        (subprocess::Exec::cmd("echo").arg("overwritten") > "redirect_test_out.txt")
            .join()
            .or_throw();
    REQUIRE(status.success());
  }

  // Read back the file
  FILE* f = fopen("redirect_test_out.txt", "r");
  REQUIRE(f != nullptr);
  char buf[256] = {};
  fgets(buf, sizeof(buf), f);
  fclose(f);
  // Normalize CRLF
  std::string out(buf);
  out.erase(std::remove(out.begin(), out.end(), '\r'), out.end());
  REQUIRE(out == "overwritten\n");

  // Overwrite with a second command
  {
    auto status =
        (subprocess::Exec::cmd("echo").arg("new content") > "redirect_test_out.txt")
            .join()
            .or_throw();
    REQUIRE(status.success());
  }

  f = fopen("redirect_test_out.txt", "r");
  REQUIRE(f != nullptr);
  memset(buf, 0, sizeof(buf));
  fgets(buf, sizeof(buf), f);
  fclose(f);
  out = buf;
  out.erase(std::remove(out.begin(), out.end(), '\r'), out.end());
  REQUIRE(out == "new content\n");

  remove("redirect_test_out.txt");
}

TEST_CASE("Exec operator>> redirects stdout to file (append)") {
  // Create or clear the file first
  {
    FILE* f = fopen("redirect_test_append.txt", "w");
    fclose(f);
  }

  // First append
  {
    auto status =
        (subprocess::Exec::cmd("echo").arg("line1") >> "redirect_test_append.txt")
            .join()
            .or_throw();
    REQUIRE(status.success());
  }

  // Second append
  {
    auto status =
        (subprocess::Exec::cmd("echo").arg("line2") >> "redirect_test_append.txt")
            .join()
            .or_throw();
    REQUIRE(status.success());
  }

  // Read back — should contain both lines
  std::string content;
  {
    FILE* f = fopen("redirect_test_append.txt", "r");
    REQUIRE(f != nullptr);
    char buf[256] = {};
    while (fgets(buf, sizeof(buf), f)) {
      content += buf;
    }
    fclose(f);
  }
  content.erase(std::remove(content.begin(), content.end(), '\r'), content.end());
  REQUIRE(content == "line1\nline2\n");

  remove("redirect_test_append.txt");
}

TEST_CASE("Exec operator< with nonexistent file returns deferred error") {
  auto result =
      (subprocess::Exec::cmd("cat") < "/definitely/nonexistent/path/file.txt")
          .popen();
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.take_error().kind == subprocess::PopenError::IoError);
}

TEST_CASE("Exec operator> to unwritable path returns deferred error") {
  // /root/output.txt should not be writable by a normal user
  auto result =
      (subprocess::Exec::cmd("echo").arg("hello") > "/root/output.txt")
          .popen();
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.take_error().kind == subprocess::PopenError::IoError);
}

TEST_CASE("Pipeline operator< redirects stdin of first stage from file") {
  {
    FILE* f = fopen("pl_in.txt", "w");
    fprintf(f, "apple\nbanana\ncherry\n");
    fclose(f);
  }

  auto pl = subprocess::Exec::cmd("cat") | subprocess::Exec::cmd("grep").arg("a");
  auto data = (pl < "pl_in.txt").capture().or_throw();
  REQUIRE(data.out == "apple\nbanana\n");
  REQUIRE(data.success());

  remove("pl_in.txt");
}

TEST_CASE("Pipeline operator> redirects stdout of last stage to file") {
  auto pl = subprocess::Exec::cmd("echo").arg("pipeline_out")
          | subprocess::Exec::cmd("cat");
  auto status = (pl > "pl_out.txt").join().or_throw();
  REQUIRE(status.success());

  FILE* f = fopen("pl_out.txt", "r");
  REQUIRE(f != nullptr);
  char buf[256] = {};
  fgets(buf, sizeof(buf), f);
  fclose(f);
  std::string out(buf);
  out.erase(std::remove(out.begin(), out.end(), '\r'), out.end());
  REQUIRE(out == "pipeline_out\n");

  remove("pl_out.txt");
}

TEST_CASE("Pipeline operator>> redirects stdout of last stage to file (append)") {
  {
    FILE* f = fopen("pl_append.txt", "w");
    fclose(f);
  }

  auto pl = subprocess::Exec::cmd("echo").arg("first") | subprocess::Exec::cmd("cat");
  auto status = (pl >> "pl_append.txt").join().or_throw();
  REQUIRE(status.success());

  status = (pl >> "pl_append.txt").join().or_throw();
  REQUIRE(status.success());

  std::string content;
  {
    FILE* f = fopen("pl_append.txt", "r");
    REQUIRE(f != nullptr);
    char buf[256] = {};
    while (fgets(buf, sizeof(buf), f)) content += buf;
    fclose(f);
  }
  content.erase(std::remove(content.begin(), content.end(), '\r'), content.end());
  REQUIRE(content == "first\nfirst\n");

  remove("pl_append.txt");
}

TEST_CASE("Combined expression: cmd < file | cmd > file") {
  {
    FILE* f = fopen("combo_in.txt", "w");
    fprintf(f, "red\ngreen\nblue\ngrey\n");
    fclose(f);
  }

  // grep 'r' < combo_in.txt | grep -v 'u' > combo_out.txt
  auto status =
      (subprocess::Exec::cmd("grep").arg("r") < "combo_in.txt"
       | subprocess::Exec::cmd("grep").arg("-v").arg("u") > "combo_out.txt")
          .join()
          .or_throw();
  REQUIRE(status.success());

  // Read output file
  std::string content;
  {
    FILE* f = fopen("combo_out.txt", "r");
    REQUIRE(f != nullptr);
    char buf[256] = {};
    while (fgets(buf, sizeof(buf), f)) content += buf;
    fclose(f);
  }
  content.erase(std::remove(content.begin(), content.end(), '\r'), content.end());
  // "red\ngreen\nblue\ngrey\n" filtered by 'r': "red\ngreen\ngrey\n"
  // minus lines with 'u': "red\ngreen\ngrey\n" (grey has no 'u')
  REQUIRE(content == "red\ngreen\ngrey\n");

  remove("combo_in.txt");
  remove("combo_out.txt");
}

TEST_CASE("Deferred error on Exec survives piping") {
  auto result =
      (subprocess::Exec::cmd("cat") < "/definitely/nonexistent/path/xyzzy"
       | subprocess::Exec::cmd("grep").arg("foo"))
          .popen();
  REQUIRE_FALSE(result.ok());
}

TEST_CASE("operator>> has correct precedence with pipe") {
  // `a | b >> "log"` should parse as `a | (b >> "log")`
  // i.e. only the last stage's stdout is redirected to the file.
  // First stage writes to stdout → goes to pipe → goes to second stage's stdin.
  // Second stage's stdout goes to file.

  {
    auto status =
        (subprocess::Exec::cmd("echo").arg("hello")
         | subprocess::Exec::cmd("cat") >> "precedence_test.txt")
            .join()
            .or_throw();
    REQUIRE(status.success());
  }

  // Read the file
  FILE* f = fopen("precedence_test.txt", "r");
  REQUIRE(f != nullptr);
  char buf[256] = {};
  fgets(buf, sizeof(buf), f);
  fclose(f);
  std::string out(buf);
  out.erase(std::remove(out.begin(), out.end(), '\r'), out.end());
  REQUIRE(out == "hello\n");

  remove("precedence_test.txt");
}

TEST_CASE("Deferred error is propagated through capture()") {
  auto result =
      (subprocess::Exec::cmd("cat") < "/definitely/nonexistent/file.txt").capture();
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.take_error().kind == subprocess::PopenError::IoError);
}

TEST_CASE("Deferred error is propagated through join()") {
  auto result =
      (subprocess::Exec::cmd("cat") < "/definitely/nonexistent/file.txt").join();
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.take_error().kind == subprocess::PopenError::IoError);
}

TEST_CASE("Deferred error is propagated through stream_stdout()") {
  auto result =
      (subprocess::Exec::cmd("cat") < "/definitely/nonexistent/file.txt")
          .stream_stdout();
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.take_error().kind == subprocess::PopenError::IoError);
}

TEST_CASE("Deferred error is propagated through stream_stdin()") {
  auto result =
      (subprocess::Exec::cmd("cat") < "/definitely/nonexistent/file.txt")
          .stream_stdin();
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.take_error().kind == subprocess::PopenError::IoError);
}
