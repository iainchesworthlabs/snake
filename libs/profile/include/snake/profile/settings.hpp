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

/// How far the phone must be tipped to steer by tilt: the usual, a quicker one for a player who finds the usual hard to
/// reach and a gentler one for a player who tips it too easily. Saved with the control scheme, in the high half of its
/// byte.
enum class TiltSensitivity : std::uint8_t
{
    NORMAL = 0,
    QUICK = 1,
    GENTLE = 2,
};

/// The number of sensitivities, for checking a stored value.
inline constexpr std::uint8_t TILT_SENSITIVITY_COUNT = 3;

/// The loudest setting of the volume. The levels run from 0 (nothing is heard) to this, in equal steps.
inline constexpr std::uint8_t VOLUME_MAX = 10;

/// What the player can change in Pause and Settings. The defaults are the values of a first run. A platform that
/// has no tilt sensor picks the first scheme it supports instead of ControlScheme::TILT, and one whose system asks
/// for less motion may start with reduced_motion on. Neither is the business of this type.
struct Settings
{
    ControlScheme controls = ControlScheme::TILT;
    TiltSensitivity tilt_sensitivity = TiltSensitivity::NORMAL;
    bool invert_axes = false;
    bool sound = true;
    bool music = true;
    std::uint8_t volume = VOLUME_MAX; // how loud, 0 to VOLUME_MAX; the sound and music switches are on top of it
    bool reduced_motion = false;

    friend bool operator==(const Settings& lhs, const Settings& rhs) noexcept = default;
};

} // namespace snake::profile
