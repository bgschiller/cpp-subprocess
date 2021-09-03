#include <catch2/catch.hpp>

#include "subprocess/Popen.hpp"

TEST_CASE("echo time") {
  auto echoR = subprocess::Popen::create({"echo", "yolo"}, subprocess::PopenConfig{});
  REQUIRE(echoR.ok());
  auto echo = echoR.take_value();
  auto exitR = echo.wait();
  REQUIRE(exitR.ok());
  auto exit = exitR.take_value();
  REQUIRE(exit.success());
}
