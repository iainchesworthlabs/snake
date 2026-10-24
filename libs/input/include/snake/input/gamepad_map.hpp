// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/input/action.hpp"
#include "snake/input/direction_selector.hpp"
#include "snake/sim/direction.hpp"

#include <cstdint>
#include <optional>

namespace snake::input
{

/// The gamepad buttons the game answers to, named by position so that they do not depend on a maker's labels. The
/// platform maps its own button codes to these and ignores every other button.
enum class GamepadButton : std::uint8_t
{
    DPAD_UP,
    DPAD_DOWN,
    DPAD_LEFT,
    DPAD_RIGHT,
    SOUTH, ///< The bottom face button: A on an Xbox pad.
    EAST,  ///< The right face button: B on an Xbox pad.
    START,
    SELECT,
};

/// The direction a button steers in, or nothing when it is not a d-pad button.
[[nodiscard]] std::optional<sim::Direction> steer_for(GamepadButton button) noexcept;

/// The action a button asks for, or nothing when it is a d-pad button: Start pauses and resumes, Select and the
/// east button open the menu, and the south button confirms.
[[nodiscard]] std::optional<Action> action_for(GamepadButton button) noexcept;

/// How the left stick becomes a direction. Both axes run from -1 to 1 with right and up positive, so the platform
/// flips the vertical axis if its stick reports down as positive. Use it with a DirectionSelector.
inline constexpr SelectorConfig STICK_SELECTOR = {.release = 0.4F, .engage = 0.6F, .switch_margin = 0.15F};

} // namespace snake::input
