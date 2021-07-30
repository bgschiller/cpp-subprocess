#include <catch2/catch.hpp>

#include "subprocess/Popen.hpp"

TEST_CASE("echo time") {
    subprocess::Popen echo({"echo", "yolo"}, subprocess::PopenConfig{});
    echo.poll();
}