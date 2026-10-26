// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/device.hpp"

#include "device/haptics.hpp"
#include "snake/input/sample_source.hpp"

#include <memory>

namespace snake::ui::device
{

// A computer: no accelerometer is used (a laptop that has one is held flat), and nothing buzzes.

std::unique_ptr<input::SampleSource> make_accelerometer()
{
    return nullptr;
}

std::unique_ptr<Haptics> make_haptics()
{
    return std::make_unique<NoHaptics>();
}

} // namespace snake::ui::device
