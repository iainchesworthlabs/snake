// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/game.hpp"

#include <format>
#include <utility>

namespace snake
{

Game::Game(std::shared_ptr<const Logger> logger) : m_Logger(std::move(logger)) {}

bool Game::start() noexcept
{
    if (m_State != GameState::READY)
    {
        return false;
    }
    m_State = GameState::RUNNING;
    return true;
}

bool Game::pause() noexcept
{
    if (m_State != GameState::RUNNING)
    {
        return false;
    }
    m_State = GameState::PAUSED;
    return true;
}

bool Game::resume() noexcept
{
    if (m_State != GameState::PAUSED)
    {
        return false;
    }
    m_State = GameState::RUNNING;
    return true;
}

bool Game::end() noexcept
{
    if (m_State != GameState::RUNNING && m_State != GameState::PAUSED)
    {
        return false;
    }
    m_State = GameState::GAME_OVER;
    return true;
}

bool Game::tick()
{
    if (m_State != GameState::RUNNING)
    {
        return false;
    }
    if (m_Logger)
    {
        m_Logger->trace(std::format("Frame {}", m_FrameCount));
    }
    ++m_FrameCount;
    return true;
}

} // namespace snake
