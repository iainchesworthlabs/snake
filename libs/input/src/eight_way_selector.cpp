// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/input/eight_way_selector.hpp"

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

} // namespace

std::optional<EightWaySelection> EightWaySelector::update(float right, float up, Direction heading) noexcept
{
    const float length = std::hypot(right, up);
    if (m_Steering ? length < m_Config.release : length < m_Config.engage)
    {
        reset();
        return std::nullopt;
    }
    m_Steering = true;

    const float across = std::fabs(right);
    const float along = std::fabs(up);
    const float larger = across > along ? across : along;
    const float smaller = across > along ? along : across;
    // The ratio of the smaller axis to the larger, which is the tangent of the angle off the nearest axis. The larger
    // axis is not zero, because the length is at least the engage threshold.
    const float ratio = smaller / larger;
    m_Diagonal = ratio >= (m_Diagonal ? m_Config.diagonal_leave : m_Config.diagonal_enter);

    const Direction horizontal = right > 0.0F ? Direction::RIGHT : Direction::LEFT;
    const Direction vertical = up > 0.0F ? Direction::UP : Direction::DOWN;
    if (m_Diagonal)
    {
        // Turn: the way across the snake's own line.
        return EightWaySelection{.direction = is_horizontal(heading) ? vertical : horizontal, .diagonal = true};
    }
    // A tie is the horizontal axis, as the other selector has it.
    return EightWaySelection{.direction = across >= along ? horizontal : vertical, .diagonal = false};
}

} // namespace snake::input
