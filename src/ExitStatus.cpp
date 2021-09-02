#include "subprocess/ExitStatus.hpp"

#include "subprocess/variant_helpers.hpp"

using namespace subprocess;

ExitStatus::ExitStatus(const ExitStatus& other)
: _state{other._state}
{ }
ExitStatus::ExitStatus(ExitStatus&& other)
: _state{std::move(other._state)}
{ }


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
  return (
    std::holds_alternative<ExitStatus::Exited>(_state) &&
    std::get<ExitStatus::Exited>(_state).code == 0);
}

std::string ExitStatus::toString() const {
  return internal::variant_to_string<ExitStatus::StateType>(_state);
}

ExitStatus& ExitStatus::operator=(ExitStatus&& other) {
  _state = std::move(other._state);
  return *this;
}
