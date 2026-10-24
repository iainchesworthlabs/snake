// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/input/direction_selector.hpp"

#include "snake/sim/direction.hpp"

#include <cmath>
#include <optional>

namespace snake::input
{

namespace
{

using sim::Direction;

constexpr bool is_horizontal(Direction direction) noexcept
{
    return direction == Direction::LEFT || direction == Direction::RIGHT;
}

/// The direction an axis value points in. Zero counts as the negative side; the caller never passes it.
constexpr Direction toward(bool horizontal, float value) noexcept
{
    if (horizontal)
    {
        return value > 0.0F ? Direction::RIGHT : Direction::LEFT;
    }
    return value > 0.0F ? Direction::UP : Direction::DOWN;
}

} // namespace

std::optional<Direction> DirectionSelector::update(float right, float up) noexcept
{
    const float right_size = std::fabs(right);
    const float up_size = std::fabs(up);

    if (m_Current)
    {
        const bool horizontal = is_horizontal(*m_Current);
        const bool positive = *m_Current == Direction::RIGHT || *m_Current == Direction::UP;
        const float own_value = horizontal ? right : up;
        const float own = positive ? own_value : -own_value;
        if (own >= m_Config.release)
        {
            const float other = horizontal ? up_size : right_size;
            if (other >= m_Config.engage && other >= own + m_Config.switch_margin)
            {
                m_Current = toward(!horizontal, horizontal ? up : right);
            }
            return m_Current;
        }
        m_Current.reset();
    }

    if (right_size >= m_Config.engage || up_size >= m_Config.engage)
    {
        const bool horizontal = right_size >= up_size;
        m_Current = toward(horizontal, horizontal ? right : up);
    }
    return m_Current;
}

} // namespace snake::input
