// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/game_state.hpp"
#include "snake/logger.hpp"

#include <cstdint>
#include <memory>

namespace snake
{

/// The state of one game. The game itself is not implemented: this holds the state and the frame
/// counter that the old loop kept in main(), and is where the rules go.
class Game
{
public:
    /// The logger may be empty, in which case nothing is logged.
    explicit Game(std::shared_ptr<const Logger> logger = nullptr);

    [[nodiscard]] GameState state() const noexcept
    {
        return m_State;
    }

    /// The number of frames ticked so far.
    [[nodiscard]] std::uint64_t frame_count() const noexcept
    {
        return m_FrameCount;
    }

    /// READY to RUNNING. Returns false, changing nothing, in any other state.
    bool start() noexcept;
    /// RUNNING to PAUSED. Returns false, changing nothing, in any other state.
    bool pause() noexcept;
    /// PAUSED to RUNNING. Returns false, changing nothing, in any other state.
    bool resume() noexcept;
    /// RUNNING or PAUSED to GAME_OVER. Returns false, changing nothing, in any other state.
    bool end() noexcept;

    /// Advances one frame while RUNNING and logs "Frame N" at trace level, with N counting from 0 as
    /// the old loop did. Returns true when a frame was counted.
    bool tick();

private:
    std::shared_ptr<const Logger> m_Logger;
    GameState m_State = GameState::READY;
    std::uint64_t m_FrameCount = 0;
};

} // namespace snake
