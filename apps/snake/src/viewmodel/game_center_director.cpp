// SPDX-License-Identifier: GPL-3.0-or-later
#include "viewmodel/game_center_director.hpp"

#include "device/game_center.hpp"
#include "snake/profile/achievements.hpp"
#include "viewmodel/game_view_model.hpp"
#include "viewmodel/profile_view_model.hpp"

#include <cstdint>

namespace snake::ui
{

GameCenterDirector::GameCenterDirector(QObject* parent) : GameCenterDirector(device::default_game_center(), parent) {}

GameCenterDirector::GameCenterDirector(device::GameCenter* game_center, QObject* parent)
    : QObject(parent), m_GameCenter(game_center)
{
    if (m_GameCenter != nullptr)
    {
        m_GameCenter->set_on_change([this] { on_availability_changed(); });
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

void GameCenterDirector::setProfile(ProfileViewModel* profile)
{
    if (profile == m_Profile)
    {
        return;
    }
    if (m_Profile != nullptr)
    {
        disconnect(m_Profile, nullptr, this, nullptr);
    }
    m_Profile = profile;
    if (m_Profile != nullptr)
    {
        connect(m_Profile, &ProfileViewModel::resultChanged, this, &GameCenterDirector::report_result);
        // A save file that was loaded may hold what was earned before, offline or without a sign-in.
        connect(m_Profile, &ProfileViewModel::saveFileChanged, this, &GameCenterDirector::report_held);
        connect(m_Profile, &QObject::destroyed, this, [this] { m_Profile = nullptr; });
    }
    emit profileChanged();
    report_held();
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

void GameCenterDirector::showAchievements()
{
    if (available())
    {
        m_GameCenter->show_achievements();
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

std::string_view GameCenterDirector::achievement_id(profile::Achievement achievement) noexcept
{
    using profile::Achievement;
    switch (achievement)
    {
    case Achievement::WRAP:
        return "com.iainchesworthlabs.snake.achievement.wrap";
    case Achievement::BONUS:
        return "com.iainchesworthlabs.snake.achievement.bonus";
    case Achievement::COMBO:
        return "com.iainchesworthlabs.snake.achievement.combo";
    case Achievement::BEST:
        return "com.iainchesworthlabs.snake.achievement.best";
    case Achievement::SPEED:
        return "com.iainchesworthlabs.snake.achievement.speed";
    case Achievement::BERRIES_100:
        return "com.iainchesworthlabs.snake.achievement.berries100";
    case Achievement::BERRIES_1000:
        return "com.iainchesworthlabs.snake.achievement.berries1000";
    case Achievement::PORTALS:
        return "com.iainchesworthlabs.snake.achievement.portals";
    case Achievement::PORTAL:
        return "com.iainchesworthlabs.snake.achievement.portal";
    case Achievement::HOT_POTATO:
        return "com.iainchesworthlabs.snake.achievement.hotpotato";
    case Achievement::SHED:
        return "com.iainchesworthlabs.snake.achievement.shed";
    case Achievement::ALL_MODES:
        return "com.iainchesworthlabs.snake.achievement.allmodes";
    }
    return {};
}

void GameCenterDirector::on_state_changed()
{
    if (m_Game->state() == GameViewModel::State::GameOver && available())
    {
        m_GameCenter->submit_score(leaderboard_id(m_Game->mode()), m_Game->points());
    }
}

void GameCenterDirector::on_availability_changed()
{
    emit availableChanged();
    report_held();
}

void GameCenterDirector::report_result()
{
    if (m_Profile != nullptr)
    {
        report(m_Profile->earnedNow());
    }
}

void GameCenterDirector::report_held()
{
    if (m_Profile != nullptr)
    {
        report(m_Profile->data().achievements);
    }
}

void GameCenterDirector::report(const profile::Achievements& earned)
{
    if (!available() || m_Profile == nullptr)
    {
        return;
    }
    const profile::Achievements& held = m_Profile->data().achievements;
    for (std::uint8_t number = 0; number < profile::ACHIEVEMENT_COUNT; ++number)
    {
        const auto achievement = static_cast<profile::Achievement>(number);
        if (earned.has(achievement))
        {
            m_GameCenter->report_achievement(achievement_id(achievement), 100.0);
        }
        else if (const auto progress = held.progress(achievement); progress && !held.has(achievement))
        {
            // A count that is not earned yet: a whole percentage, rounded down, and nothing at zero.
            const auto percent = static_cast<std::uint64_t>(progress->current) * 100 / progress->target;
            if (percent > 0)
            {
                m_GameCenter->report_achievement(achievement_id(achievement), static_cast<double>(percent));
            }
        }
    }
}

} // namespace snake::ui
