// Use a custom main so we can force stdout to be unbuffered on Windows.
// Without this, a crash via __fastfail (STATUS_STACK_BUFFER_OVERRUN) kills
// the process before the CRT flushes Catch2's output buffer, making it
// impossible to see which test failed.
#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>
#include <cstdio>

int main(int argc, char* argv[]) {
  // Unbuffered stdout: every character is written immediately to the OS pipe
  // that CTest reads, so output survives a hard crash.
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::setvbuf(stderr, nullptr, _IONBF, 0);
  return Catch::Session().run(argc, argv);
}
