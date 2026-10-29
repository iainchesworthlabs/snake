// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/sim/direction.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace snake::sim
{

/// One thing the player did: a turn, or (in Hot potato) shedding the tail from the bomb. `tick` is the number of ticks
/// that had been played when it was accepted, so a turn is the one the next tick (the tick with that number, counting
/// from 0) takes from the input buffer, and a shed happens just before that tick.
struct InputEvent
{
    std::uint32_t tick = 0;
    Direction direction = Direction::UP; // for a turn
    bool shed = false;                   // a shed, and `direction` means nothing

    friend constexpr bool operator==(const InputEvent& lhs, const InputEvent& rhs) noexcept = default;
};

/// The turns and sheds of one game in the order they were made. It holds only those the game accepted, because a turn
/// the input buffer drops changes nothing and drops again on replay. The capacity is fixed, so recording allocates
/// nothing.
class InputLog
{
public:
    static constexpr std::size_t CAPACITY = 8192;

    /// Adds a turn. Returns false, changing nothing, when the log is full or `tick` is earlier than the last turn's.
    bool record(std::uint32_t tick, Direction direction) noexcept
    {
        if (m_Count == CAPACITY || (m_Count > 0 && tick < m_Events[m_Count - 1].tick))
        {
            return false;
        }
        m_Events[m_Count++] = InputEvent{.tick = tick, .direction = direction};
        return true;
    }

    /// Adds a shed, with the same rules as record().
    bool record_shed(std::uint32_t tick) noexcept
    {
        if (m_Count == CAPACITY || (m_Count > 0 && tick < m_Events[m_Count - 1].tick))
        {
            return false;
        }
        m_Events[m_Count++] = InputEvent{.tick = tick, .shed = true};
        return true;
    }

    [[nodiscard]] std::span<const InputEvent> events() const noexcept
    {
        return {m_Events.data(), m_Count};
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return m_Count;
    }

    [[nodiscard]] bool full() const noexcept
    {
        return m_Count == CAPACITY;
    }

    friend bool operator==(const InputLog& lhs, const InputLog& rhs) noexcept
    {
        if (lhs.m_Count != rhs.m_Count)
        {
            return false;
        }
        for (std::size_t index = 0; index < lhs.m_Count; ++index)
        {
            if (lhs.m_Events[index] != rhs.m_Events[index])
            {
                return false;
            }
        }
        return true;
    }

private:
    std::array<InputEvent, CAPACITY> m_Events{};
    std::size_t m_Count = 0;
};

} // namespace snake::sim
