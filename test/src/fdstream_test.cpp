#ifndef _WIN32
#include <cerrno>
#include <unistd.h>
#endif

#include <catch2/catch.hpp>

#include "vendor/fdstream.hpp"

TEST_CASE("fdistream::close() closes the underlying fd") {
#ifndef _WIN32
  // Create a temporary pipe, write a byte so there's something to read,
  // then wrap the read end in a fdistream and call close().
  int pipefd[2];
  REQUIRE(::pipe(pipefd) == 0);

  // Write one byte to the pipe so the read end isn't at EOF.
  const char data = 'x';
  REQUIRE(::write(pipefd[1], &data, 1) == 1);
  ::close(pipefd[1]);  // Close write end so the read end eventually gets EOF.

  {
    boost::fdistream stream(pipefd[0]);
    stream.close();
  }

  // The fd should now be closed. A subsequent read must fail with EBADF.
  char buf[1];
  ssize_t ret = ::read(pipefd[0], buf, sizeof(buf));
  REQUIRE(ret == -1);
  REQUIRE(errno == EBADF);
#else
  // Windows fdstream close tests are not yet implemented.
  REQUIRE(true);
#endif
}
