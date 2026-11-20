// SPDX-License-Identifier: GPL-3.0-or-later
#include "viewmodel/game_center_director.hpp"

#include "device/game_center.hpp"
#include "viewmodel/game_view_model.hpp"

namespace snake::ui
{

GameCenterDirector::GameCenterDirector(QObject* parent) : GameCenterDirector(device::default_game_center(), parent) {}

GameCenterDirector::GameCenterDirector(device::GameCenter* game_center, QObject* parent)
    : QObject(parent), m_GameCenter(game_center)
{
    if (m_GameCenter != nullptr)
    {
        m_GameCenter->set_on_change([this] { emit availableChanged(); });
    }
}

GameCenterDirector::~GameCenterDirector()
{
    if (m_GameCenter != nullptr)
    {
        m_GameCenter->set_on_change({});
    }
}

void GameCenterDirector::setGame(GameViewModel* game)
{
    if (game == m_Game)
    {
        return;
    }
    if (m_Game != nullptr)
    {
        disconnect(m_Game, nullptr, this, nullptr);
    }
    m_Game = game;
    if (m_Game != nullptr)
    {
        connect(m_Game, &GameViewModel::stateChanged, this, &GameCenterDirector::on_state_changed);
    }
    emit gameChanged();
}

bool GameCenterDirector::available() const
{
    return m_GameCenter != nullptr && m_GameCenter->available();
}

void GameCenterDirector::showLeaderboard(GameViewModel::Mode mode)
{
    if (available())
    {
        m_GameCenter->show_leaderboard(leaderboard_id(mode));
    }
}

std::string_view GameCenterDirector::leaderboard_id(GameViewModel::Mode mode) noexcept
{
    switch (mode)
    {
    case GameViewModel::Mode::Easy:
        return "com.iainchesworthlabs.snake.leaderboard.easy";
    case GameViewModel::Mode::Classic:
        return "com.iainchesworthlabs.snake.leaderboard.classic";
    case GameViewModel::Mode::Portals:
        return "com.iainchesworthlabs.snake.leaderboard.portals";
    case GameViewModel::Mode::HotPotato:
        return "com.iainchesworthlabs.snake.leaderboard.hotpotato";
    }
    return "com.iainchesworthlabs.snake.leaderboard.classic";
}

void GameCenterDirector::on_state_changed()
{
    if (m_Game->state() == GameViewModel::State::GameOver && available())
    {
        m_GameCenter->submit_score(leaderboard_id(m_Game->mode()), m_Game->points());
    }
}

} // namespace snake::ui
