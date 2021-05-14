#include "subprocess/type_name.hpp"

#include <catch2/catch.hpp>

#include "subprocess/Redirection.hpp"

using namespace subprocess;
using namespace subprocess::internal;

TEST_CASE("type-name") {
  REQUIRE(get_type_name<None>() == "subprocess::internal::None");

  Redirection r{None()};
  REQUIRE(r.toString() == "subprocess::Redirection::None");
}
