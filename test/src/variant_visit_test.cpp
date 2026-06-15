#include <catch2/catch.hpp>

#include <string>
#include <variant>

#include "subprocess/ExitStatus.hpp"
#include "subprocess/Redirection.hpp"
#include "subprocess/detail/ChildState.hpp"
#include "subprocess/detail/variant_helpers.hpp"

using namespace subprocess;

TEST_CASE("Redirection::visit") {
  SECTION("visits Pipe and returns an int") {
    Redirection r{Redirection::Pipe{}};
    int result = r.visit(internal::overloaded{
        [](const Redirection::Pipe&) { return 1; },
        [](const Redirection::FileDescriptor&) { return 2; },
        [](const Redirection::Merge&) { return 3; },
        [](const Redirection::None&) { return 4; },
    });
    REQUIRE(result == 1);
  }

  SECTION("visits FileDescriptor and returns an int") {
    // Use release() to avoid closing fd 0 (stdin).
    Redirection::FileDescriptor fd(0);
    fd.release();
    Redirection r{std::move(fd)};
    int result = r.visit(internal::overloaded{
        [](const Redirection::Pipe&) { return 1; },
        [](const Redirection::FileDescriptor&) { return 2; },
        [](const Redirection::Merge&) { return 3; },
        [](const Redirection::None&) { return 4; },
    });
    REQUIRE(result == 2);
  }

  SECTION("visits Merge and returns a string") {
    Redirection r{Redirection::Merge{}};
    std::string result = r.visit(internal::overloaded{
        [](const Redirection::Pipe&) -> std::string { return "pipe"; },
        [](const Redirection::FileDescriptor&) -> std::string { return "fd"; },
        [](const Redirection::Merge&) -> std::string { return "merge"; },
        [](const Redirection::None&) -> std::string { return "none"; },
    });
    REQUIRE(result == "merge");
  }

  SECTION("visits None and returns bool") {
    Redirection r{Redirection::None{}};
    bool result = r.visit(internal::overloaded{
        [](const Redirection::Pipe&) { return false; },
        [](const Redirection::FileDescriptor&) { return false; },
        [](const Redirection::Merge&) { return false; },
        [](const Redirection::None&) { return true; },
    });
    REQUIRE(result == true);
  }

  SECTION("visitor captures by reference") {
    Redirection r{Redirection::Pipe{}};
    int side_effect = 0;
    r.visit(internal::overloaded{
        [&](const Redirection::Pipe&) { side_effect = 42; },
        [](const Redirection::FileDescriptor&) {},
        [](const Redirection::Merge&) {},
        [](const Redirection::None&) {},
    });
    REQUIRE(side_effect == 42);
  }
}

TEST_CASE("ChildState::visit") {
  SECTION("visits Preparing") {
    ChildState state{ChildState::Preparing{}};
    int result = state.visit(internal::overloaded{
        [](const ChildState::Preparing&) { return 1; },
        [](const ChildState::Running&) { return 2; },
        [](const ChildState::Finished&) { return 3; },
    });
    REQUIRE(result == 1);
  }

  SECTION("visits Running") {
    ChildState state{ChildState::Running{42}};
    int result = state.visit(internal::overloaded{
        [](const ChildState::Preparing&) { return 1; },
        [](const ChildState::Running& r) {
          REQUIRE(r.pid == 42);
          return 2;
        },
        [](const ChildState::Finished&) { return 3; },
    });
    REQUIRE(result == 2);
  }

  SECTION("visits Finished") {
    ChildState state{ChildState::Finished{ExitStatus::Exited{7}}};
    int result = state.visit(internal::overloaded{
        [](const ChildState::Preparing&) { return 1; },
        [](const ChildState::Running&) { return 2; },
        [](const ChildState::Finished& finished) {
          REQUIRE(finished.exit_status.is_a<ExitStatus::Exited>());
          REQUIRE(finished.exit_status.get<ExitStatus::Exited>().code == 7);
          return 3;
        },
    });
    REQUIRE(result == 3);
  }

  SECTION("return type deduced from visitor — std::string") {
    ChildState state{ChildState::Preparing{}};
    std::string result = state.visit(internal::overloaded{
        [](const ChildState::Preparing&) -> std::string { return "preparing"; },
        [](const ChildState::Running&) -> std::string { return "running"; },
        [](const ChildState::Finished&) -> std::string { return "finished"; },
    });
    REQUIRE(result == "preparing");
  }
}

TEST_CASE("ExitStatus::visit") {
  SECTION("visits Exited") {
    ExitStatus status{ExitStatus::Exited{42}};
    int result = status.visit(internal::overloaded{
        [](const ExitStatus::Exited& e) {
          REQUIRE(e.code == 42);
          return 1;
        },
        [](const ExitStatus::Signaled&) { return 2; },
        [](const ExitStatus::Other&) { return 3; },
        [](const ExitStatus::Undetermined&) { return 4; },
    });
    REQUIRE(result == 1);
  }

  SECTION("visits Signaled") {
    ExitStatus status{ExitStatus::Signaled{9}};
    int result = status.visit(internal::overloaded{
        [](const ExitStatus::Exited&) { return 1; },
        [](const ExitStatus::Signaled& s) {
          REQUIRE(s.signal == 9);
          return 2;
        },
        [](const ExitStatus::Other&) { return 3; },
        [](const ExitStatus::Undetermined&) { return 4; },
    });
    REQUIRE(result == 2);
  }

  SECTION("visits Other") {
    ExitStatus status{ExitStatus::Other{99}};
    std::string result = status.visit(internal::overloaded{
        [](const ExitStatus::Exited&) -> std::string { return "exited"; },
        [](const ExitStatus::Signaled&) -> std::string { return "signaled"; },
        [](const ExitStatus::Other& o) -> std::string {
          REQUIRE(o.code == 99);
          return "other";
        },
        [](const ExitStatus::Undetermined&) -> std::string { return "undetermined"; },
    });
    REQUIRE(result == "other");
  }

  SECTION("visits Undetermined") {
    ExitStatus status{ExitStatus::Undetermined{}};
    bool result = status.visit(internal::overloaded{
        [](const ExitStatus::Exited&) { return false; },
        [](const ExitStatus::Signaled&) { return false; },
        [](const ExitStatus::Other&) { return false; },
        [](const ExitStatus::Undetermined&) { return true; },
    });
    REQUIRE(result == true);
  }

  SECTION("can use visit where is_a/get was previously needed") {
    ExitStatus status{ExitStatus::Exited{5}};
    // Before: if (status.is_a<ExitStatus::Exited>()) { int c = status.get<ExitStatus::Exited>().code; }
    // Now: single visit call
    status.visit(internal::overloaded{
        [](const ExitStatus::Exited& e) { REQUIRE(e.code == 5); },
        [](const ExitStatus::Signaled&) { FAIL("unexpected"); },
        [](const ExitStatus::Other&) { FAIL("unexpected"); },
        [](const ExitStatus::Undetermined&) { FAIL("unexpected"); },
    });
  }
}
