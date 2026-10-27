// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/haptics.hpp"
#include "snake/input/sample_source.hpp"

#include <functional>
#include <memory>

namespace snake::ui::device
{

// What a phone has that a computer does not. The functions are declared here and defined once for each operating
// system, in src/device/<os>/ (CMake picks the directory, as the standard asks: no #if in the code). A computer answers
// that it has neither.

/// The accelerometer of the device, as a source of samples for the tilt processor, or nothing when there is none.
[[nodiscard]] std::unique_ptr<input::SampleSource> make_accelerometer();

/// What makes the accelerometer: by default make_accelerometer() above. Tests give a recording instead, and an empty
/// function puts the device's own back.
using AccelerometerFactory = std::function<std::unique_ptr<input::SampleSource>()>;
void set_accelerometer_factory(AccelerometerFactory factory);

/// The accelerometer the game reads: the factory's, or the device's own.
[[nodiscard]] std::unique_ptr<input::SampleSource> accelerometer();

/// The motor of the device, or a Haptics that does nothing. Never null.
[[nodiscard]] std::unique_ptr<Haptics> make_haptics();

/// Holds the display in the orientation it is in now (true), so that tilting the phone to steer cannot turn the screen,
/// or lets it follow the device again (false). A computer's window does not turn, so it does nothing there. Call it
/// from the thread of the user interface.
void lock_orientation(bool locked);

} // namespace snake::ui::device
