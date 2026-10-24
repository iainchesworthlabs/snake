// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/input/gamepad_map.hpp"

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

// In the order of GamepadButton.
constexpr std::array<Binding, 8> BINDINGS = {
    steering(Direction::UP), steering(Direction::DOWN), steering(Direction::LEFT),    steering(Direction::RIGHT),
    acting(Action::CONFIRM), acting(Action::MENU),      acting(Action::PAUSE_RESUME), acting(Action::MENU),
};

const Binding& binding_for(GamepadButton button) noexcept
{
    return BINDINGS[static_cast<std::size_t>(button) % BINDINGS.size()];
}

} // namespace

std::optional<Direction> steer_for(GamepadButton button) noexcept
{
    return binding_for(button).steer;
}

std::optional<Action> action_for(GamepadButton button) noexcept
{
    return binding_for(button).action;
}

} // namespace snake::input
