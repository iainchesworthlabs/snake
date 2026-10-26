// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/input/sample_source.hpp"

#include <memory>

namespace snake::ui::device
{

/// The accelerometer through Qt Sensors, as a source of samples. The phones use it. It collects the readings as they
/// arrive on the main thread and gives them back one at a time, oldest first, so the tilt processor sees every one.
/// Returns nothing when the device has no accelerometer, or the system does not let the program have it.
[[nodiscard]] std::unique_ptr<input::SampleSource> make_qt_accelerometer();

} // namespace snake::ui::device
