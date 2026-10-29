// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/device.hpp"

#include "device/gamepad.hpp"
#include "device/haptics.hpp"
#include "device/sdl_gamepad.hpp"
#include "snake/input/sample_source.hpp"

#include <memory>

namespace snake::ui::device
{

// A computer: no accelerometer is used (a laptop that has one is held flat), nothing buzzes, and the window does not
// turn.

std::unique_ptr<input::SampleSource> make_accelerometer()
{
    return nullptr;
}

std::unique_ptr<GamepadSource> make_gamepad_source()
{
    return make_sdl_gamepad_source();
}

std::unique_ptr<Haptics> make_haptics()
{
    return std::make_unique<NoHaptics>();
}

void lock_orientation(bool /*locked*/) {}

} // namespace snake::ui::device
