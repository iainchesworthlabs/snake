// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/linux/float_math.hpp"

#include <cmath>

namespace snake::ui::device
{

// This file is compiled with -fno-math-errno (apps/snake/CMakeLists.txt). Without it the optimiser turns the square
// root of a double that came from a float back into a call of `sqrtf`, and that call is the one that is wrapped to come
// here.

namespace
{

constexpr double widen(float value) noexcept
{
    return static_cast<double>(value);
}

} // namespace

float float_acos(float value) noexcept
{
    return static_cast<float>(std::acos(widen(value)));
}

float float_asin(float value) noexcept
{
    return static_cast<float>(std::asin(widen(value)));
}

float float_atan(float value) noexcept
{
    return static_cast<float>(std::atan(widen(value)));
}

float float_atan2(float y, float x) noexcept
{
    return static_cast<float>(std::atan2(widen(y), widen(x)));
}

float float_cos(float value) noexcept
{
    return static_cast<float>(std::cos(widen(value)));
}

float float_exp(float value) noexcept
{
    return static_cast<float>(std::exp(widen(value)));
}

float float_log(float value) noexcept
{
    return static_cast<float>(std::log(widen(value)));
}

float float_log10(float value) noexcept
{
    return static_cast<float>(std::log10(widen(value)));
}

float float_pow(float base, float exponent) noexcept
{
    return static_cast<float>(std::pow(widen(base), widen(exponent)));
}

float float_sin(float value) noexcept
{
    return static_cast<float>(std::sin(widen(value)));
}

float float_sqrt(float value) noexcept
{
    return static_cast<float>(std::sqrt(widen(value)));
}

float float_tan(float value) noexcept
{
    return static_cast<float>(std::tan(widen(value)));
}

} // namespace snake::ui::device

extern "C"
{

    // NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
    float __wrap_acosf(float value)
    {
        return snake::ui::device::float_acos(value);
    }

    float __wrap_asinf(float value)
    {
        return snake::ui::device::float_asin(value);
    }

    float __wrap_atanf(float value)
    {
        return snake::ui::device::float_atan(value);
    }

    float __wrap_atan2f(float y, float x)
    {
        return snake::ui::device::float_atan2(y, x);
    }

    float __wrap_cosf(float value)
    {
        return snake::ui::device::float_cos(value);
    }

    float __wrap_expf(float value)
    {
        return snake::ui::device::float_exp(value);
    }

    float __wrap_logf(float value)
    {
        return snake::ui::device::float_log(value);
    }

    float __wrap_log10f(float value)
    {
        return snake::ui::device::float_log10(value);
    }

    float __wrap_powf(float base, float exponent)
    {
        return snake::ui::device::float_pow(base, exponent);
    }

    float __wrap_sinf(float value)
    {
        return snake::ui::device::float_sin(value);
    }

    float __wrap_sqrtf(float value)
    {
        return snake::ui::device::float_sqrt(value);
    }

    float __wrap_tanf(float value)
    {
        return snake::ui::device::float_tan(value);
    }
    // NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)

} // extern "C"
