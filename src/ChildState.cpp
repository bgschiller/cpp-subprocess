#include "subprocess/detail/ChildState.hpp"

#include "subprocess/detail/variant_helpers.hpp"

using namespace subprocess;
using namespace subprocess::internal;

ChildState& ChildState::operator=(ChildState&& other) {
  _state = std::move(other._state);
  return *this;
}

ChildState::ChildState(ChildState&& other)
    : _state{ std::move(other._state) } { }

ChildState::ChildState(const ChildState& other)
    : _state{ other._state } { }
