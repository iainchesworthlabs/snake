// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/device.hpp"
#include "snake/input/sample_source.hpp"

#include <memory>
#include <utility>

namespace snake::ui::device
{

namespace
{

AccelerometerFactory g_factory;

} // namespace

void set_accelerometer_factory(AccelerometerFactory factory)
{
    g_factory = std::move(factory);
}

std::unique_ptr<input::SampleSource> accelerometer()
{
    return g_factory ? g_factory() : make_accelerometer();
}

} // namespace snake::ui::device
