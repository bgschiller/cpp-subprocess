// Use a custom main so we can force stdout to be unbuffered on Windows.
// Without this, a crash via __fastfail (STATUS_STACK_BUFFER_OVERRUN) kills
// the process before the CRT flushes Catch2's output buffer, making it
// impossible to see which test failed.
#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>
#include <cstdio>

int main(int argc, char* argv[]) {
  // Unbuffered I/O: every character is written immediately so output survives
  // a hard crash via __fastfail / STATUS_STACK_BUFFER_OVERRUN.
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::setvbuf(stderr, nullptr, _IONBF, 0);
  // Also force std::cout / std::cerr to flush after every operation.
  std::ios::sync_with_stdio(true);
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  return Catch::Session().run(argc, argv);
}
