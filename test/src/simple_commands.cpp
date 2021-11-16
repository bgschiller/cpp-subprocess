#include <catch2/catch.hpp>
#include <fcntl.h>
#include <optional>
#include <string>
#include <unistd.h>

#include "subprocess/Popen.hpp"
#include "subprocess/Redirection.hpp"

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
      config.stdin = Redirection::FileDescriptor(fruitsFd);
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


  }

  // SECTION("porcelain") {
  //   auto res = Exec("echo yolo") | Exec("cat") > Redirection::Write("output.txt")
  // }
}
