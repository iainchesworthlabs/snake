// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/sim/direction.hpp"

#include <optional>

namespace snake::input
{

/// The numbers that decide how a thumb on an on-screen joystick becomes a direction. The axes are the knob's place as a
/// fraction of its travel, from -1 to 1, with right and up positive.
struct EightWayConfig
{
    /// The knob must go this far from the middle to start steering (a fraction of its travel).
    float engage = 0.0F;
    /// ... and steering stops below this. It is at most `engage`; the gap keeps a thumb that wobbles near the edge
    /// steady.
    float release = 0.0F;
    /// The smaller axis, as a fraction of the larger, at which the knob is read as pointing diagonally: 0.47 is 25
    /// degrees off an axis, so the diagonal directions have 40 degrees each and the four straight ones 50.
    float diagonal_enter = 0.0F;
    /// ... and it goes back to a straight direction below this (20 degrees). The gap stops a thumb on the edge from
    /// changing its mind.
    float diagonal_leave = 0.0F;
};

/// What a thumb on the joystick means: a direction, and whether the knob points diagonally.
struct EightWaySelection
{
    sim::Direction direction = sim::Direction::UP;
    bool diagonal = false;
};

/// Turns the knob of an on-screen joystick into a direction, in eight ways: the four straight ones, and the four
/// diagonals.
///
/// A snake can only go in four directions, so a diagonal means "turn": the knob at north-east asks for the way the
/// snake is not going on. Heading east, north-east is north. Heading north, it is east. A thumb held on a diagonal
/// therefore steers a staircase, one step each way, as the snake's own heading changes under it, and a thumb that
/// flicks to the diagonal and back moves the snake one row aside. Straight is as it says, wherever the snake is heading
/// (and a turn back into itself is the game's to refuse).
///
/// The direction is worked out from the heading the caller gives, which must be the one the snake will have when the
/// turn is made (the last queued turn, if there is one). The class only remembers whether it is steering and whether
/// the knob is on a diagonal, for the two gaps above.
class EightWaySelector
{
public:
    explicit EightWaySelector(EightWayConfig config) noexcept : m_Config(config) {}

    /// Takes the knob's place and returns what it asks for, or nothing when the knob is near the middle.
    std::optional<EightWaySelection> update(float right, float up, sim::Direction heading) noexcept;

    /// Forgets that the thumb was down, as when it lifts or the game restarts.
    void reset() noexcept
    {
        m_Steering = false;
        m_Diagonal = false;
    }

    [[nodiscard]] const EightWayConfig& config() const noexcept
    {
        return m_Config;
    }

private:
    EightWayConfig m_Config;
    bool m_Steering = false;
    bool m_Diagonal = false;
};

/// The joystick of the game: engages at 30 percent of the way and lets go at 20, which a thumb does without effort,
/// with the diagonals as above.
inline constexpr EightWayConfig TOUCH_JOYSTICK = {
    .engage = 0.30F, .release = 0.20F, .diagonal_enter = 0.47F, .diagonal_leave = 0.36F};

} // namespace snake::input
