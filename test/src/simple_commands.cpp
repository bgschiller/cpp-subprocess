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
    int fruitsFd = open("fruits.tmp", O_RDONLY);
    PopenConfig config;
    config.stdin = Redirection::File(fruitsFd);
    config.stdout = Redirection::Pipe();
    auto grep = Popen::create({"grep", "apple"}, config).or_throw();
    close(fruitsFd);

    auto exit = grep.wait().or_throw();
    REQUIRE(exit.success());

    REQUIRE(grep.std_out->slurp() == "apple\npineapple\n");
  }

  // SECTION("porcelain") {
  //   auto res = Exec("echo yolo") | Exec("cat") > Redirection::File("output.txt")
  // }
}
