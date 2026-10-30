// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/input/eight_way_selector.hpp"
#include "snake/sim/direction.hpp"
#include "snake/sim/simulation.hpp"

#include <optional>

namespace snake::input
{

/// An on-screen joystick that steers a game: the rule of EightWaySelector, applied to the game it is steering. The
/// platform reports where the thumb is and when it lifts, and asks again after each tick; this class says which turn,
/// if any, to hand to the game.
///
/// The heading a turn is judged against is the one the snake will have when the turn is made: the last turn waiting, or
/// the heading it has. A diagonal is a turn across the snake's line, and the line changes with each turn, so while a
/// turn is waiting a diagonal is left until the tick has taken it (held on a diagonal, the knob then asks for the next
/// one: a staircase). A straight direction is offered at once, so that a flick to the diagonal and back, inside one
/// tick, is two turns. The game's input buffer drops a repeat of the direction already in force.
class JoystickSteering
{
public:
    explicit JoystickSteering(EightWayConfig config = TOUCH_JOYSTICK) noexcept : m_Selector(config) {}

    /// Takes the thumb's place on the joystick (a fraction of the knob's travel, right and up positive) without
    /// steering, for a platform that has a reason not to steer yet (the game is paused).
    void hold(float right, float up) noexcept;

    /// hold(), then again(): the turn to make now, or nothing when the knob is near the middle, the game is not
    /// running, or the turn has to wait.
    std::optional<sim::Direction> move(float right, float up, const sim::Simulation& game) noexcept;

    /// A tick has been taken: asks again for a thumb that is still down, which is how a thumb held on a diagonal goes
    /// on steering. Returns nothing when the thumb is not down.
    std::optional<sim::Direction> again(const sim::Simulation& game) noexcept;

    /// The thumb lifted, or the game started over.
    void release() noexcept;

    [[nodiscard]] bool held() const noexcept
    {
        return m_Held;
    }

private:
    EightWaySelector m_Selector;
    float m_Right = 0.0F;
    float m_Up = 0.0F;
    bool m_Held = false;
};

} // namespace snake::input
