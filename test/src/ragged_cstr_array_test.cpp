#include <catch2/catch.hpp>

#include "subprocess/RaggedCstrArray.hpp"

using namespace subprocess;

TEST_CASE("RaggedCstrArray") {
  GIVEN("An initially empty array") {
    RaggedCstrArray arr;
    WHEN("we turn it into a char*") {
      auto chstr = arr.asCharStar();
      THEN("it contains only the nullptr") {
        REQUIRE(chstr != nullptr);
        REQUIRE(*chstr == nullptr);
      }
      AND_WHEN("a string is later added") {
        arr.push("potato");
        THEN("the char* comes back with potato, then a nullptr") {
          auto chstr2 = arr.asCharStar();
          REQUIRE(chstr2 != nullptr);
          REQUIRE(chstr2[0] == std::string("potato"));
          REQUIRE(chstr2[1] == nullptr);
        }
      }
    }

    WHEN("a string is added") {
      arr.push("tomato");
      THEN("the char star contains a c-string equal to the value and a nullptr cap") {
        auto chstr = arr.asCharStar();
        REQUIRE(chstr != nullptr);
        REQUIRE(chstr[0] == std::string("tomato"));
        REQUIRE(chstr[1] == nullptr);
      }
    }
  }

  GIVEN("a handful of string to initialize") {
    const std::string redFish{ "red fish" };
    const std::string blueFish{ "blue fish" };
    RaggedCstrArray arr{ "one fish", std::string("two fish"), redFish, blueFish };

    THEN("the char star contains a c-string for each entry, plus a nullptr") {
      auto chstr = arr.asCharStar();
      REQUIRE(chstr != nullptr);
      REQUIRE(chstr[0] == std::string("one fish"));
      REQUIRE(chstr[1] == std::string("two fish"));
      REQUIRE(chstr[2] == std::string("red fish"));
      REQUIRE(chstr[3] == std::string("blue fish"));
      REQUIRE(chstr[4] == nullptr);
    }
  }
}
