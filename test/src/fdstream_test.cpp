#ifndef _WIN32
#include <array>
#include <cerrno>
#include <unistd.h>
#endif

#include <catch2/catch.hpp>

#include "vendor/fdstream.hpp"

TEST_CASE("fdistream::close() closes the underlying fd") {
#ifndef _WIN32
  // Create a temporary pipe, write a byte so there's something to read,
  // then wrap the read end in a fdistream and call close().
  std::array<int, 2> pipefd{};
  REQUIRE(::pipe(pipefd.data()) == 0);

  // Write one byte to the pipe so the read end isn't at EOF.
  const char data = 'x';
  REQUIRE(::write(pipefd[1], &data, 1) == 1);
  ::close(pipefd[1]);  // Close write end so the read end eventually gets EOF.

  {
    boost::fdistream stream(pipefd[0]);
    stream.close();
  }

  // The fd should now be closed. A subsequent read must fail with EBADF.
  std::array<char, 1> buf{};
  ssize_t ret = ::read(pipefd[0], buf.data(), buf.size());
  REQUIRE(ret == -1);
  REQUIRE(errno == EBADF);
#else
  // Windows fdstream close tests are not yet implemented.
  REQUIRE(true);
#endif
}
