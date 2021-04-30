#include "subprocess/ExitStatus.hpp"

bool subprocess::ExitStatus::success() const {
  return (
      std::holds_alternative<subprocess::Exited>(*this) &&
      std::get<subprocess::Exited>(*this).code == 0);
}
