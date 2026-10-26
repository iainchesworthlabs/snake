// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/haptics.hpp"
#include "snake/input/sample_source.hpp"

#include <memory>

namespace snake::ui::device
{

// What a phone has that a computer does not. The functions are declared here and defined once for each operating
// system, in src/device/<os>/ (CMake picks the directory, as the standard asks: no #if in the code). A computer answers
// that it has neither.

/// The accelerometer of the device, as a source of samples for the tilt processor, or nothing when there is none.
[[nodiscard]] std::unique_ptr<input::SampleSource> make_accelerometer();

/// The motor of the device, or a Haptics that does nothing. Never null.
[[nodiscard]] std::unique_ptr<Haptics> make_haptics();

} // namespace snake::ui::device
