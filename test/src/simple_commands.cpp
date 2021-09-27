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

  // SECTION("pipe input and output") {
  //   PopenConfig config;
  //   config.stdin = Redirection::Pipe();
  //   config.stdout = Redirection::Pipe();
  //   auto grepR = Popen::create({"grep", "apple"}, config);
  //   REQUIRE(grepR.ok());
  //   auto grep = grepR.take_value();
  //   REQUIRE(grep.poll() == std::nullopt);

  //   std::string fruits = "apple\nbanana\npineapple\nlemon\n";
  //   fprintf(grep.std_in, "%s", fruits.c_str());
  //   fclose(grep.std_in);

  //   auto exitR = grep.wait();
  //   REQUIRE(exitR.ok());

  //   auto exit = exitR.take_value();
  //   REQUIRE(exit.success());

  //   char buf[20];
  //   fread(buf, 20, 1, grep.std_out);
  //   REQUIRE(buf == std::string("apple\npineapple\n"));
  // }

  // SECTION("input from file") {
  //   FILE* fruits = fopen("fruits.tmp", "w");
  //   fprintf(fruits, "apple\nbanana\npineapple\nlemon\n");
  //   fclose(fruits);
  //   int fruitsFd = open("fruits.tmp", O_RDONLY);
  //   PopenConfig config;
  //   config.stdin = Redirection::File(fruitsFd);
  //   config.stdout = Redirection::Pipe();
  //   auto grep = Popen::create({"grep", "apple"}, config).or_throw();
  //   close(fruitsFd);

  //   auto exit = grep.wait().or_throw();
  //   REQUIRE(exit.success());

  //   char buf[20];
  //   fread(buf, 20, 1, grep.std_out);
  //   REQUIRE(buf == std::string("apple\npineapple\n"));
  // }

  // SECTION("porcelain") {
  //   auto res = Exec("echo yolo") | Exec("cat") > Redirection::File("output.txt")
  // }
}
