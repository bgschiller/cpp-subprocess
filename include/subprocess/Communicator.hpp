#ifndef SUBPROCESS_COMMUNICATOR_H_
#define SUBPROCESS_COMMUNICATOR_H_

#include <fstream>
#include <optional>

#include "PopenError.hpp"

namespace subprocess {
  namespace raw {
    struct RawCommunicator {
      std::optional<std::ofstream> stdin;
      std::optional<std::ifstream> stdout;
      std::optional<std::ifstream> stderr;

      std::string input_data;
      size_t input_pos{ 0 };

      std::optional<PopenError> do_read(
          std::ifstream& source_ref, std::string& dest, std::optional<size_t> size_limit,
          size_t total_read) {
        size_t bytesToRead = 4096;
        std::array<char, bytesToRead> buf;
        if (size_limit.has_value()) {
          if (total_read >= *size_limit) return std::nullopt;
          if (size_limit - total_read < buf.size()) { bytesToRead = *size_limit - total_read; }
        }
        source_ref.read(&buf[0], bytesToRead);
      }
    };
  }  // namespace raw
}  // namespace subprocess
#endif
