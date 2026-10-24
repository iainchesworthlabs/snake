// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/sim/direction.hpp"
#include "snake/sim/ring_buffer.hpp"

#include <cstddef>
#include <optional>

namespace snake::sim
{

/// The direction inputs waiting for the next ticks. Every input method (tilt, swipe, keys, gamepad) feeds one.
///
/// A quick pair of presses must both count, so up to two inputs wait, and the game takes one per tick. Each input
/// is judged against the direction the snake will be travelling once the input before it has been applied, not
/// against the direction it travels now. Otherwise "up" then "left" while moving right would be fine on its own
/// but "up" then "down" could slip through and turn the snake back into itself.
class InputBuffer
{
public:
    static constexpr std::size_t CAPACITY = 2;

    /// Queues `input`. `heading` is the direction the snake travels now. Returns false, changing nothing, when the
    /// input is dropped: it repeats the direction the snake will already be travelling, it would reverse that
    /// direction, or the buffer is full.
    bool push(Direction heading, Direction input) noexcept;

    /// The next input for the tick that is starting, or nothing when none is waiting.
    [[nodiscard]] std::optional<Direction> pop() noexcept;

    [[nodiscard]] std::size_t size() const noexcept
    {
        return m_Queue.size();
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_Queue.empty();
    }

    /// The input that is `position` places from the front, 0 being the next to be taken. Needs position < size().
    [[nodiscard]] Direction at(std::size_t position) const noexcept
    {
        return m_Queue[position];
    }

    /// Drops every waiting input, for a new game or after a pause.
    void clear() noexcept
    {
        m_Queue.clear();
    }

private:
    RingBuffer<Direction, CAPACITY> m_Queue;
};

} // namespace snake::sim
