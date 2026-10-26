// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

namespace snake::profile
{

/// How the player steers on a touch device. Keyboard and gamepad always work where they exist, so they are not a
/// choice here. The numbers are written into the save file, so a new scheme goes at the end.
enum class ControlScheme : std::uint8_t
{
    TILT = 0,
    SWIPE = 1,
    DPAD = 2,
    JOYSTICK = 3,
};

/// The number of control schemes, for checking a stored value.
inline constexpr std::uint8_t CONTROL_SCHEME_COUNT = 4;

/// What the player can change in Pause and Settings. The defaults are the values of a first run. A platform that
/// has no tilt sensor picks the first scheme it supports instead of ControlScheme::TILT, and one whose system asks
/// for less motion may start with reduced_motion on. Neither is the business of this type.
struct Settings
{
    ControlScheme controls = ControlScheme::TILT;
    bool invert_axes = false;
    bool sound = true;
    bool music = true;
    bool reduced_motion = false;

    friend bool operator==(const Settings& lhs, const Settings& rhs) noexcept = default;
};

} // namespace snake::profile
