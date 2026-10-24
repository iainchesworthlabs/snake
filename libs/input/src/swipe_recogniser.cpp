// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/input/swipe_recogniser.hpp"

#include "snake/sim/direction.hpp"

#include <cmath>
#include <cstdint>
#include <optional>

namespace snake::input
{

void SwipeRecogniser::press(std::int32_t pointer, TouchPoint at) noexcept
{
    if (m_Tracking && pointer != m_Pointer)
    {
        return;
    }
    m_Tracking = true;
    m_Pointer = pointer;
    m_Anchor = at;
}

std::optional<sim::Direction> SwipeRecogniser::move(std::int32_t pointer, TouchPoint at) noexcept
{
    if (!m_Tracking || pointer != m_Pointer)
    {
        return std::nullopt;
    }
    const float dx = at.x - m_Anchor.x;
    const float dy = at.y - m_Anchor.y;
    const bool horizontal = std::fabs(dx) >= std::fabs(dy);
    if (std::fabs(horizontal ? dx : dy) < m_Config.min_distance)
    {
        return std::nullopt;
    }
    m_Anchor = at;
    if (horizontal)
    {
        return dx > 0.0F ? sim::Direction::RIGHT : sim::Direction::LEFT;
    }
    return dy > 0.0F ? sim::Direction::DOWN : sim::Direction::UP;
}

void SwipeRecogniser::release(std::int32_t pointer) noexcept
{
    if (m_Tracking && pointer == m_Pointer)
    {
        m_Tracking = false;
    }
}

} // namespace snake::input
