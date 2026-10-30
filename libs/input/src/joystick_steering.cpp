// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/input/joystick_steering.hpp"

#include "snake/sim/direction.hpp"
#include "snake/sim/simulation.hpp"

#include <optional>

namespace snake::input
{

void JoystickSteering::hold(float right, float up) noexcept
{
    m_Right = right;
    m_Up = up;
    m_Held = true;
}

std::optional<sim::Direction> JoystickSteering::move(float right, float up, const sim::Simulation& game) noexcept
{
    hold(right, up);
    return again(game);
}

std::optional<sim::Direction> JoystickSteering::again(const sim::Simulation& game) noexcept
{
    if (!m_Held || !game.running())
    {
        return std::nullopt;
    }
    const auto& pending = game.pending_turns();
    const sim::Direction heading = pending.empty() ? game.heading() : pending.at(pending.size() - 1);
    const auto selection = m_Selector.update(m_Right, m_Up, heading);
    if (!selection)
    {
        return std::nullopt;
    }
    if (selection->diagonal && !pending.empty())
    {
        return std::nullopt;
    }
    return selection->direction;
}

void JoystickSteering::release() noexcept
{
    m_Held = false;
    m_Right = 0.0F;
    m_Up = 0.0F;
    m_Selector.reset();
}

} // namespace snake::input
