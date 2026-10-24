// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/input/key_map.hpp"

#include "binding.hpp"
#include "snake/input/action.hpp"
#include "snake/sim/direction.hpp"

#include <array>
#include <cstddef>
#include <optional>

namespace snake::input
{

namespace
{

using sim::Direction;

// In the order of Key.
constexpr std::array<Binding, 11> BINDINGS = {
    steering(Direction::UP),      steering(Direction::DOWN), steering(Direction::LEFT), steering(Direction::RIGHT),
    steering(Direction::UP),      steering(Direction::LEFT), steering(Direction::DOWN), steering(Direction::RIGHT),
    acting(Action::PAUSE_RESUME), acting(Action::MENU),      acting(Action::CONFIRM),
};

const Binding& binding_for(Key key) noexcept
{
    return BINDINGS[static_cast<std::size_t>(key) % BINDINGS.size()];
}

} // namespace

std::optional<Direction> steer_for(Key key) noexcept
{
    return binding_for(key).steer;
}

std::optional<Action> action_for(Key key) noexcept
{
    return binding_for(key).action;
}

} // namespace snake::input
