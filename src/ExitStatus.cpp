#include "subprocess/ExitStatus.hpp"

#include "subprocess/variant_helpers.hpp"

std::string subprocess::internal::Exited::toString() const {
  return "subprocess::ExitStatus::Exited(" + std::to_string(code) + ")";
}

std::string subprocess::internal::Signaled::toString() const {
  return "subprocess::ExitStatus::Signaled(" + std::to_string(signal) + ")";
}

std::string subprocess::internal::Other::toString() const {
  return "subprocess::ExitStatus::Other(" + std::to_string(code) + ")";
}

bool subprocess::ExitStatus::success() const {
  return (
      std::holds_alternative<subprocess::ExitStatus::Exited>(*this) &&
      std::get<subprocess::ExitStatus::Exited>(*this).code == 0);
}

std::string subprocess::ExitStatus::toString() const {
  return subprocess::internal::variant_to_string(*this);
}
