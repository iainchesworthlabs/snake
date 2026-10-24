// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/input/action.hpp"
#include "snake/sim/direction.hpp"

#include <optional>

namespace snake::input
{

/// What one key or button does: it steers, or it asks for an action. The tables in key_map.cpp and gamepad_map.cpp
/// are made of these.
struct Binding
{
    std::optional<sim::Direction> steer;
    std::optional<Action> action;
};

constexpr Binding steering(sim::Direction direction) noexcept
{
    return {direction, std::nullopt};
}

constexpr Binding acting(Action action) noexcept
{
    return {std::nullopt, action};
}

} // namespace snake::input
