#include "subprocess/ChildState.hpp"

#include "subprocess/variant_helpers.hpp"

using namespace subprocess;
using namespace subprocess::internal;

Result<const std::nullopt_t> ChildState::match(
  std::function<Result<const std::nullopt_t>(const Preparing&)> preparing_case,
  std::function<Result<const std::nullopt_t>(const Running&)> running_case,
  std::function<Result<const std::nullopt_t>(const Finished&)> finished_case
) const {
  if (is_a<Preparing>()) return preparing_case(get<Preparing>());
  if (is_a<Running>()) return running_case(get<Running>());
  if (is_a<Finished>()) return finished_case(get<Finished>());
  return std::nullopt;
}

ChildState& ChildState::operator=(ChildState&& other) {
  _state = std::move(other._state);
  return *this;
}
