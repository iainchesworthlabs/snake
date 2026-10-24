// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cstdint>

namespace snake::sim
{

/// The fixed-timestep part of the game loop. The platform tells it how much time has passed since the last frame, and
/// it says how many ticks are due. It reads no clock, so the same loop runs on the desktop, the phones and the ESP32-P4
/// and a test can play a game in any order of frame times.
///
/// Time is kept in microseconds, so frames of 16.67 ms do not lose a fraction each. The interval can change between
/// ticks, because the game speeds up as it goes; each tick takes the interval that applies when it is due.
///
/// ```
/// pacer.add(elapsed_us);
/// while (session.state() == GameState::RUNNING && pacer.due(session.tick_interval_ms()))
/// {
///     session.tick();
/// }
/// ```
class TickPacer
{
public:
    /// The most time that is kept. After a stall (a window dragged, a breakpoint, a slow frame) the game picks up from
    /// here instead of racing through every tick it missed. At the fastest interval of 70 ms this allows three ticks.
    static constexpr std::uint32_t MAX_PENDING_US = 250'000;

    /// Time that has passed since the last call, in microseconds.
    void add(std::uint32_t elapsed_us) noexcept
    {
        const std::uint64_t total = static_cast<std::uint64_t>(m_PendingUs) + elapsed_us;
        m_PendingUs = static_cast<std::uint32_t>(std::min<std::uint64_t>(total, MAX_PENDING_US));
    }

    /// True once per tick that is due. Each true takes the interval out of the time that has passed.
    [[nodiscard]] bool due(std::uint16_t interval_ms) noexcept
    {
        const std::uint32_t interval_us = static_cast<std::uint32_t>(interval_ms) * 1000U;
        if (m_PendingUs < interval_us)
        {
            return false;
        }
        m_PendingUs -= interval_us;
        return true;
    }

    /// The time that has passed since the last tick, for drawing between ticks.
    [[nodiscard]] std::uint32_t pending_us() const noexcept
    {
        return m_PendingUs;
    }

    /// Forgets the time that has passed, for a new game and for the first frame after a pause.
    void reset() noexcept
    {
        m_PendingUs = 0;
    }

private:
    std::uint32_t m_PendingUs = 0;
};

} // namespace snake::sim
