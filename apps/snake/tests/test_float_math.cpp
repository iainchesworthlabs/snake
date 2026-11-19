// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/linux/float_math.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <limits>
#include <numbers>

using Catch::Matchers::WithinULP;
using namespace snake::ui::device;

namespace
{

constexpr int STEPS = 200;
constexpr float NOT_A_NUMBER = std::numeric_limits<float>::quiet_NaN();
constexpr float INFINITY_VALUE = std::numeric_limits<float>::infinity();

/// The values from -limit to limit, `STEPS` each side of zero.
float sample(int step, float limit)
{
    return limit * static_cast<float>(step) / static_cast<float>(STEPS);
}

} // namespace

// The float functions of the C library, which these stand in for, are good to one unit in the last place or better, and
// so are the double functions rounded to a float. The two can differ by one.

TEST_CASE("The square root of a float is the correctly rounded one", "[float_math]")
{
    for (int step = 0; step <= 10000; ++step)
    {
        const float value = static_cast<float>(step) * 0.37F;
        CHECK(float_sqrt(value) == std::sqrt(value));
    }
    CHECK(float_sqrt(0.0F) == 0.0F);
    CHECK(float_sqrt(4.0F) == 2.0F);
}

TEST_CASE("The square root of a negative float or a NaN is a NaN, and ends", "[float_math]")
{
    CHECK(std::isnan(float_sqrt(-1.0F)));
    CHECK(std::isnan(float_sqrt(NOT_A_NUMBER)));
    CHECK(float_sqrt(INFINITY_VALUE) == INFINITY_VALUE);
}

TEST_CASE("The angle of a point is the one of the float function", "[float_math]")
{
    for (int step = 0; step < 360; ++step)
    {
        const double angle = static_cast<double>(step) * std::numbers::pi / 180.0;
        const float y = static_cast<float>(std::sin(angle)) * 7.5F;
        const float x = static_cast<float>(std::cos(angle)) * 7.5F;
        CHECK_THAT(float_atan2(y, x), WithinULP(std::atan2(y, x), 1));
    }
    CHECK(float_atan2(0.0F, 1.0F) == 0.0F);
    CHECK_THAT(float_atan2(1.0F, 0.0F), WithinULP(static_cast<float>(std::numbers::pi / 2.0), 1));
    CHECK_THAT(float_atan2(0.0F, -1.0F), WithinULP(static_cast<float>(std::numbers::pi), 1));
}

TEST_CASE("The inverse functions are those of the float functions", "[float_math]")
{
    for (int step = -STEPS; step <= STEPS; ++step)
    {
        const float unit = sample(step, 1.0F);
        CHECK_THAT(float_asin(unit), WithinULP(std::asin(unit), 1));
        CHECK_THAT(float_acos(unit), WithinULP(std::acos(unit), 1));
        const float wide = sample(step, 50.0F);
        CHECK_THAT(float_atan(wide), WithinULP(std::atan(wide), 1));
    }
    CHECK(std::isnan(float_asin(1.5F)));
    CHECK(std::isnan(float_acos(-1.5F)));
}

TEST_CASE("The trigonometric functions are those of the float functions", "[float_math]")
{
    for (int step = -STEPS; step <= STEPS; ++step)
    {
        // Not within a few units of a multiple of pi or half of it, where the result is near zero (or large), and one
        // unit in the last place of the argument is a large part of the result.
        const float angle = sample(step, 1.4F);
        CHECK_THAT(float_sin(angle), WithinULP(std::sin(angle), 1));
        CHECK_THAT(float_cos(angle), WithinULP(std::cos(angle), 1));
        CHECK_THAT(float_tan(angle), WithinULP(std::tan(angle), 1));
    }
}

TEST_CASE("The exponential, logarithms and power are those of the float functions", "[float_math]")
{
    for (int step = -STEPS; step <= STEPS; ++step)
    {
        const float exponent = sample(step, 20.0F);
        CHECK_THAT(float_exp(exponent), WithinULP(std::exp(exponent), 1));
        const float positive = std::exp(exponent);
        CHECK_THAT(float_log(positive), WithinULP(std::log(positive), 2));
        CHECK_THAT(float_log10(positive), WithinULP(std::log10(positive), 2));
        CHECK_THAT(float_pow(2.5F, sample(step, 8.0F)), WithinULP(std::pow(2.5F, sample(step, 8.0F)), 1));
    }
    CHECK(float_log(1.0F) == 0.0F);
    CHECK(float_log10(1000.0F) == 3.0F);
    CHECK(float_pow(2.0F, 10.0F) == 1024.0F);
    CHECK(std::isnan(float_log(-1.0F)));
    CHECK(float_log(0.0F) == -INFINITY_VALUE);
}

TEST_CASE("The names the linker substitutes reach the same functions", "[float_math]")
{
    CHECK(__wrap_acosf(0.5F) == float_acos(0.5F));
    CHECK(__wrap_asinf(0.5F) == float_asin(0.5F));
    CHECK(__wrap_atanf(0.5F) == float_atan(0.5F));
    CHECK(__wrap_atan2f(1.0F, 2.0F) == float_atan2(1.0F, 2.0F));
    CHECK(__wrap_cosf(0.5F) == float_cos(0.5F));
    CHECK(__wrap_expf(0.5F) == float_exp(0.5F));
    CHECK(__wrap_logf(0.5F) == float_log(0.5F));
    CHECK(__wrap_log10f(0.5F) == float_log10(0.5F));
    CHECK(__wrap_powf(2.0F, 0.5F) == float_pow(2.0F, 0.5F));
    CHECK(__wrap_sinf(0.5F) == float_sin(0.5F));
    CHECK(__wrap_sqrtf(2.0F) == float_sqrt(2.0F));
    CHECK(__wrap_tanf(0.5F) == float_tan(0.5F));
}
