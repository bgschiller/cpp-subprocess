#ifndef SUBPROCESS_EXCEPTION_H_
#define SUBPROCESS_EXCEPTION_H_
#include <exception>

namespace subprocess {
  struct SubprocessException: public std::runtime_error {
    using std::runtime_error::runtime_error;
  };
}

#endif
