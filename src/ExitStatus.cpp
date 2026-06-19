#include "subprocess/ExitStatus.hpp"

#include <string>

#include "subprocess/detail/variant_helpers.hpp"

using namespace subprocess;

ExitStatus::ExitStatus(ExitStatus&& other) noexcept
    : _state{ other._state } { }

std::string ExitStatus::Exited::toString() const {
  return "subprocess::ExitStatus::Exited(" + std::to_string(code) + ")";
}

std::string ExitStatus::Signaled::toString() const {
  return "subprocess::ExitStatus::Signaled(" + std::to_string(signal) + ")";
}

std::string ExitStatus::Other::toString() const {
  return "subprocess::ExitStatus::Other(" + std::to_string(code) + ")";
}

bool ExitStatus::success() const {
  return visit(
      subprocess::internal::overloaded{
          [](const Exited& exited) { return exited.code == 0; },
          [](const Signaled&) { return false; },
          [](const Other&) { return false; },
          [](const Undetermined&) { return false; },
      });
}

std::string ExitStatus::toString() const {
  return internal::variant_to_string<ExitStatus::StateType>(_state);
}

ExitStatus& ExitStatus::operator=(ExitStatus&& other) noexcept {
  if (this != &other) {
    _state = other._state;
  }
  return *this;
}
