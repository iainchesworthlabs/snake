// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/linux/float_math.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <limits>
#include <numbers>

using Catch::Matchers::WithinULP;
using snake::ui::device::float_asin;
using snake::ui::device::float_atan2;
using snake::ui::device::float_sqrt;

namespace
{

constexpr int STEPS = 360;

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
    CHECK(float_sqrt(2.0F) == std::sqrt(2.0F));
}

TEST_CASE("The square root of a negative float or a NaN is a NaN, and ends", "[float_math]")
{
    CHECK(std::isnan(float_sqrt(-1.0F)));
    CHECK(std::isnan(float_sqrt(std::numeric_limits<float>::quiet_NaN())));
    CHECK(float_sqrt(std::numeric_limits<float>::infinity()) == std::numeric_limits<float>::infinity());
}

TEST_CASE("The angle of a point is the one of the float function", "[float_math]")
{
    for (int step = 0; step < STEPS; ++step)
    {
        const double angle = static_cast<double>(step) * std::numbers::pi / 180.0;
        const float y = static_cast<float>(std::sin(angle)) * 7.5F;
        const float x = static_cast<float>(std::cos(angle)) * 7.5F;
        CHECK_THAT(float_atan2(y, x), WithinULP(std::atan2(y, x), 1));
    }
    CHECK(float_atan2(0.0F, 1.0F) == 0.0F);
    CHECK_THAT(float_atan2(1.0F, 0.0F), WithinULP(static_cast<float>(std::numbers::pi / 2.0), 1));
    CHECK_THAT(float_atan2(0.0F, -1.0F), WithinULP(static_cast<float>(std::numbers::pi), 1));
    CHECK_THAT(float_atan2(1.0F, 1.0F), WithinULP(static_cast<float>(std::numbers::pi / 4.0), 1));
}

TEST_CASE("The arc sine of a float is the one of the float function", "[float_math]")
{
    for (int step = -100; step <= 100; ++step)
    {
        const float value = static_cast<float>(step) / 100.0F;
        CHECK_THAT(float_asin(value), WithinULP(std::asin(value), 1));
    }
    CHECK_THAT(float_asin(1.0F), WithinULP(static_cast<float>(std::numbers::pi / 2.0), 1));
    CHECK(std::isnan(float_asin(1.5F)));
}

TEST_CASE("The names the linker substitutes reach the same functions", "[float_math]")
{
    CHECK(__wrap_sqrtf(2.0F) == float_sqrt(2.0F));
    CHECK(__wrap_asinf(0.5F) == float_asin(0.5F));
    CHECK(__wrap_atan2f(1.0F, 2.0F) == float_atan2(1.0F, 2.0F));
}
