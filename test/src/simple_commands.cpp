#include <catch2/catch.hpp>

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

  SECTION("pipe") {
    PopenConfig config;
    config.stdout = Redirection::Pipe();
    auto echoR = Popen::create({"echo", "yolo"}, config);
    REQUIRE(echoR.ok());
    auto echo = echoR.take_value();
    char buf[16];
    fgets(buf, 16, echo.std_out);
    REQUIRE(buf == std::string("yolo\n"));
    fclose(echo.std_out);
    auto exitR = echo.wait();
    auto exit = exitR.take_value();
    REQUIRE(exit.success());
  }
}
