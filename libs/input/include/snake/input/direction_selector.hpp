// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/sim/direction.hpp"

#include <optional>

namespace snake::input
{

/// The three numbers that decide when a pair of axis values becomes a direction. They share one unit, which is
/// degrees for tilt and a fraction of full deflection for a stick.
struct SelectorConfig
{
    /// A direction lets go once its own axis falls below this. Anything below it on both axes is "level".
    float release = 0.0F;
    /// A direction is set once an axis reaches this. It is at least `release`; the gap is the band where a direction
    /// that is already set stays set, and a new one is not yet.
    float engage = 0.0F;
    /// The axis in control keeps it until the other axis is this much larger.
    float switch_margin = 0.0F;
};

/// Turns two signed axis values into at most one direction, with the rules the tilt and the left stick share:
///
/// - nothing is set until an axis reaches `engage`; when both do, the larger wins (a tie goes to the horizontal axis);
/// - a set direction stays while its axis is at least `release` in the same sense, so a value drifting between
///   `release` and `engage` does not flicker;
/// - the axis in control keeps priority until the other axis is at least `engage` and `switch_margin` larger, so a
///   diagonal hold never alternates between two directions.
///
/// "Right" is positive towards the right of the screen and "up" is positive towards the top. The caller converts.
class DirectionSelector
{
public:
    explicit DirectionSelector(SelectorConfig config) noexcept : m_Config(config) {}

    /// Takes the latest axis values and returns the direction in force, or nothing when the input is level.
    std::optional<sim::Direction> update(float right, float up) noexcept;

    /// The direction in force after the last update.
    [[nodiscard]] std::optional<sim::Direction> current() const noexcept
    {
        return m_Current;
    }

    /// Forgets the direction in force, as when the game restarts.
    void reset() noexcept
    {
        m_Current.reset();
    }

    [[nodiscard]] const SelectorConfig& config() const noexcept
    {
        return m_Config;
    }

private:
    SelectorConfig m_Config;
    std::optional<sim::Direction> m_Current;
};

} // namespace snake::input
