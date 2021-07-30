#include "subprocess/ExitStatus.hpp"

#include "subprocess/variant_helpers.hpp"

std::string subprocess::ExitStatus::Exited::toString() const {
  return "subprocess::ExitStatus::Exited(" + std::to_string(code) + ")";
}

std::string subprocess::ExitStatus::Signaled::toString() const {
  return "subprocess::ExitStatus::Signaled(" + std::to_string(signal) + ")";
}

std::string subprocess::ExitStatus::Other::toString() const {
  return "subprocess::ExitStatus::Other(" + std::to_string(code) + ")";
}

bool subprocess::ExitStatus::success() const {
  return (
    std::holds_alternative<subprocess::ExitStatus::Exited>(_state) &&
    std::get<subprocess::ExitStatus::Exited>(_state).code == 0);
}

std::string subprocess::ExitStatus::toString() const {
  return subprocess::internal::variant_to_string<ExitStatus::StateType>(_state);
}
