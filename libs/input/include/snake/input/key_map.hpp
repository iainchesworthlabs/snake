// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/input/action.hpp"
#include "snake/sim/direction.hpp"

#include <cstdint>
#include <optional>

namespace snake::input
{

/// The keys the game answers to. The platform maps its own key codes to these (both Enter keys give ENTER) and
/// ignores every other key. The values are only for the tables in this library.
enum class Key : std::uint8_t
{
    ARROW_UP,
    ARROW_DOWN,
    ARROW_LEFT,
    ARROW_RIGHT,
    W,
    A,
    S,
    D,
    SPACE,
    ESCAPE,
    ENTER,
};

/// The direction a key steers in, or nothing when it is not a steering key: arrow keys and W A S D.
[[nodiscard]] std::optional<sim::Direction> steer_for(Key key) noexcept;

/// The action a key asks for, or nothing when it is a steering key: Space pauses and resumes, Escape opens the
/// menu and Enter confirms.
[[nodiscard]] std::optional<Action> action_for(Key key) noexcept;

} // namespace snake::input
