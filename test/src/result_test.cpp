#include "subprocess/Result.hpp"

#include <catch2/catch.hpp>
#include <string>

#include "subprocess/PopenError.hpp"
#include "subprocess/SubprocessException.hpp"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static subprocess::PopenError make_error(const std::string& msg) {
  return subprocess::PopenError{ subprocess::PopenError::LogicError, msg };
}

// ---------------------------------------------------------------------------
// Basic construction / ok()
// ---------------------------------------------------------------------------

TEST_CASE("Result: basic construction") {
  SECTION("success holds value") {
    subprocess::Result<int> r{ 42 };
    REQUIRE(r.ok());
  }

  SECTION("error does not hold value") {
    subprocess::Result<int> r{ make_error("oops") };
    REQUIRE_FALSE(r.ok());
  }
}

// ---------------------------------------------------------------------------
// operator bool
// ---------------------------------------------------------------------------

TEST_CASE("Result: operator bool") {
  SECTION("truthy on success") {
    subprocess::Result<int> r{ 7 };
    REQUIRE(static_cast<bool>(r));
    if (r) {
      SUCCEED("entered if-block");
    } else {
      FAIL("should have entered if-block");
    }
  }

  SECTION("falsy on error") {
    subprocess::Result<int> r{ make_error("bad") };
    REQUIRE_FALSE(static_cast<bool>(r));
    if (!r) {
      SUCCEED("entered else-block");
    } else {
      FAIL("should have entered else-block");
    }
  }
}

// ---------------------------------------------------------------------------
// operator* and operator->
// ---------------------------------------------------------------------------

TEST_CASE("Result: operator* returns reference to value") {
  subprocess::Result<std::string> r{ std::string("hello") };
  REQUIRE(*r == "hello");
  // mutate via reference
  *r = "world";
  REQUIRE(*r == "world");
}

TEST_CASE("Result: operator-> gives pointer access to value members") {
  subprocess::Result<std::string> r{ std::string("hello") };
  REQUIRE(r->size() == 5u);
}

// ---------------------------------------------------------------------------
// or_throw — must throw by value, not pointer
// ---------------------------------------------------------------------------

TEST_CASE("Result: or_throw on success returns value") {
  subprocess::Result<int> r{ 99 };
  REQUIRE(r.or_throw() == 99);
}

TEST_CASE("Result: or_throw on error throws SubprocessException by value") {
  subprocess::Result<int> r{ make_error("bang") };
  // The thrown object must be catchable by reference (i.e. thrown by value).
  REQUIRE_THROWS_AS(r.or_throw(), subprocess::SubprocessException);
  // Must NOT be catchable as a pointer (old `throw new` bug).
  bool caught_as_pointer = false;
  try {
    subprocess::Result<int> r2{ make_error("bang2") };
    r2.or_throw();
  } catch (subprocess::SubprocessException*) {
    caught_as_pointer = true;
  } catch (subprocess::SubprocessException&) {
    // good — thrown by value
  }
  REQUIRE_FALSE(caught_as_pointer);
}

// ---------------------------------------------------------------------------
// map()
// ---------------------------------------------------------------------------

TEST_CASE("Result: map transforms success value") {
  subprocess::Result<int> r{ 3 };
  auto r2 = r.map([](int x) { return x * 2; });
  REQUIRE(r2.ok());
  REQUIRE(*r2 == 6);
}

TEST_CASE("Result: map propagates error unchanged") {
  subprocess::Result<int> r{ make_error("map-err") };
  auto r2 = r.map([](int x) { return x * 2; });
  REQUIRE_FALSE(r2.ok());
  REQUIRE(r2.take_error().message == "map-err");
}

TEST_CASE("Result: map can change type") {
  subprocess::Result<int> r{ 42 };
  subprocess::Result<std::string> r2 = r.map([](int x) { return std::to_string(x); });
  REQUIRE(r2.ok());
  REQUIRE(*r2 == "42");
}

// ---------------------------------------------------------------------------
// and_then()
// ---------------------------------------------------------------------------

TEST_CASE("Result: and_then chains successful operations") {
  subprocess::Result<int> r{ 10 };
  auto r2 = r.and_then([](int x) -> subprocess::Result<int> { return x + 5; });
  REQUIRE(r2.ok());
  REQUIRE(*r2 == 15);
}

TEST_CASE("Result: and_then propagates first error") {
  subprocess::Result<int> r{ make_error("first-err") };
  bool called = false;
  auto r2 = r.and_then([&called](int x) -> subprocess::Result<int> {
    called = true;
    return x + 1;
  });
  REQUIRE_FALSE(r2.ok());
  REQUIRE_FALSE(called);
  REQUIRE(r2.take_error().message == "first-err");
}

TEST_CASE("Result: and_then propagates error from chained function") {
  subprocess::Result<int> r{ 5 };
  auto r2 = r.and_then([](int) -> subprocess::Result<int> { return make_error("chain-err"); });
  REQUIRE_FALSE(r2.ok());
  REQUIRE(r2.take_error().message == "chain-err");
}

TEST_CASE("Result: and_then can change type") {
  subprocess::Result<int> r{ 7 };
  subprocess::Result<std::string> r2 =
      r.and_then([](int x) -> subprocess::Result<std::string> { return std::to_string(x); });
  REQUIRE(r2.ok());
  REQUIRE(*r2 == "7");
}
