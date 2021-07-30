#include "subprocess/type_name.hpp"

#include <catch2/catch.hpp>

#include "subprocess/ExitStatus.hpp"
#include "subprocess/Redirection.hpp"

using namespace subprocess;
using namespace subprocess::internal;

TEST_CASE("type-name") {
  REQUIRE(get_type_name<Redirection::None>() == "subprocess::Redirection::None");

  Redirection r {Redirection::None{}};
  REQUIRE(r.toString() == "subprocess::Redirection::None");

  ExitStatus e {ExitStatus::Signaled { 9 }};
  REQUIRE(e.toString() == "subprocess::ExitStatus::Signaled(9)");
}
