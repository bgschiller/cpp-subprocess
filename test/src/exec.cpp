#include <catch2/catch.hpp>
#include <fcntl.h>
#include <optional>
#include <string>
#include <unistd.h>

#include "subprocess/Exec.hpp"
#include "subprocess/ExitStatus.hpp"
#include "subprocess/Popen.hpp"
#include "subprocess/Redirection.hpp"
#include "subprocess/posix.hpp"

using namespace subprocess;

TEST_CASE("exec") {
  SECTION("Simple cmds") {
    // TODO: implement Exec::join() and add assertions here
    Exec::cmd("echo").arg("yolo");
  }
}


