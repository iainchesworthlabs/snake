// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/sim/edge_rule.hpp"
#include "snake/sim/variant.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace snake::sim
{

/// A game mode. A mode sets the edge rule, the variant (the extra rules) and the speed curve. The numbers are written
/// into save files and replays, so a new mode goes at the end.
enum class Mode : std::uint8_t
{
    EASY,
    CLASSIC,
    PORTALS,
    HOT_POTATO,
};

/// The number of modes, for checking a stored value.
inline constexpr std::size_t MODE_COUNT = 4;

[[nodiscard]] constexpr EdgeRule edge_rule_of(Mode mode) noexcept
{
    return mode == Mode::EASY ? EdgeRule::WRAP : EdgeRule::WALLS;
}

[[nodiscard]] constexpr Variant variant_of(Mode mode) noexcept
{
    switch (mode)
    {
    case Mode::PORTALS:
        return Variant::PORTALS;
    case Mode::HOT_POTATO:
        return Variant::HOT_POTATO;
    case Mode::EASY:
    case Mode::CLASSIC:
        break;
    }
    return Variant::PLAIN;
}

/// How fast the game runs. The tick starts at `start_ms`, gets `step_ms` shorter for every food eaten and never
/// goes below `floor_ms`. The values are whole milliseconds.
struct SpeedCurve
{
    std::uint16_t start_ms = 0;
    std::uint16_t step_ms = 0;
    std::uint16_t floor_ms = 0;
};

/// Easy is slow, Classic and Portals are quick, and Hot potato starts a little slower and stops sooner, so that there
/// is time to shed.
[[nodiscard]] constexpr SpeedCurve speed_curve_of(Mode mode) noexcept
{
    switch (mode)
    {
    case Mode::EASY:
        return {.start_ms = 180, .step_ms = 2, .floor_ms = 120};
    case Mode::HOT_POTATO:
        return {.start_ms = 160, .step_ms = 3, .floor_ms = 80};
    case Mode::CLASSIC:
    case Mode::PORTALS:
        break;
    }
    return {.start_ms = 150, .step_ms = 3, .floor_ms = 70};
}

/// The time between ticks after `foods_eaten` foods. Normal food and bonus food both count.
[[nodiscard]] constexpr std::uint16_t tick_interval_ms(SpeedCurve curve, std::size_t foods_eaten) noexcept
{
    // The curve reaches its floor after at most start_ms / step_ms foods, so a long game cannot overflow.
    const std::size_t steps =
        curve.step_ms == 0 ? 0 : std::min<std::size_t>(foods_eaten, curve.start_ms / curve.step_ms);
    const auto interval = static_cast<std::uint16_t>(curve.start_ms - (steps * curve.step_ms));
    return std::max(interval, curve.floor_ms);
}

[[nodiscard]] constexpr std::uint16_t tick_interval_ms(Mode mode, std::size_t foods_eaten) noexcept
{
    return tick_interval_ms(speed_curve_of(mode), foods_eaten);
}

} // namespace snake::sim
