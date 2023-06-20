#include <catch2/catch.hpp>
#include <fcntl.h>
#include <optional>
#include <string>
#include <unistd.h>

#include "subprocess/Popen.hpp"
#include "subprocess/Redirection.hpp"
#include "subprocess/posix.hpp"

TEST_CASE("exec") {
  SECTION("Simple cmds") {
    PopenResult<ExitStatus> res = Exec::cmd("echo").arg("yolo").join();
  }
}


