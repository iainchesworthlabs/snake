// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/linux/float_math.hpp"

#include <cmath>

namespace snake::ui::device
{

// This file is compiled with -fno-math-errno (apps/snake/CMakeLists.txt). Without it the optimiser turns the square
// root of a double that came from a float back into a call of `sqrtf`, and that call is the one that is wrapped to come
// here.

float float_atan2(float y, float x) noexcept
{
    return static_cast<float>(std::atan2(static_cast<double>(y), static_cast<double>(x)));
}

float float_asin(float value) noexcept
{
    return static_cast<float>(std::asin(static_cast<double>(value)));
}

float float_sqrt(float value) noexcept
{
    return static_cast<float>(std::sqrt(static_cast<double>(value)));
}

} // namespace snake::ui::device

extern "C"
{

    // NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
    float __wrap_atan2f(float y, float x)
    {
        return snake::ui::device::float_atan2(y, x);
    }

    float __wrap_asinf(float value)
    {
        return snake::ui::device::float_asin(value);
    }

    float __wrap_sqrtf(float value)
    {
        return snake::ui::device::float_sqrt(value);
    }
    // NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)

} // extern "C"
