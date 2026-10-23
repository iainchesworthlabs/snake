// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/input_buffer.hpp"

namespace snake::sim
{

bool InputBuffer::push(Direction heading, Direction input) noexcept
{
    const Direction pending = m_Queue.empty() ? heading : m_Queue.back();
    if (input == pending || is_reversal(pending, input))
    {
        return false;
    }
    // A full queue refuses the input and says so.
    return m_Queue.push_back(input);
}

std::optional<Direction> InputBuffer::pop() noexcept
{
    return m_Queue.take_front();
}

} // namespace snake::sim
