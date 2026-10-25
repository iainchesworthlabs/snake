// SPDX-License-Identifier: GPL-3.0-or-later
#include "viewmodel/profile_view_model.hpp"

#include "snake/profile/leaderboard.hpp"
#include "snake/profile/unlocks.hpp"
#include "snake/sim/mode.hpp"
#include "snake/sim/scoring.hpp"
#include "viewmodel/game_view_model.hpp"

#include <QString>
#include <cstdint>

namespace snake::ui
{

namespace
{

constexpr sim::Mode to_sim(GameViewModel::Mode mode) noexcept
{
    return mode == GameViewModel::Mode::Easy ? sim::Mode::EASY : sim::Mode::CLASSIC;
}

constexpr ModeListModel::Entry entry_of(profile::Unlockable unlockable) noexcept
{
    return unlockable == profile::Unlockable::PORTALS ? ModeListModel::Entry::Portals : ModeListModel::Entry::HotPotato;
}

QString six_digits(std::uint32_t score)
{
    const auto digits = sim::score_digits(score);
    return QString::fromLatin1(digits.data(), static_cast<qsizetype>(digits.size()));
}

} // namespace

ProfileViewModel::ProfileViewModel(QObject* parent) : QObject(parent), m_Modes(m_Data.unlocks, this) {}

void ProfileViewModel::setGame(GameViewModel* game)
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
        connect(m_Game, &GameViewModel::stateChanged, this, &ProfileViewModel::on_state_changed);
        connect(m_Game, &GameViewModel::modeChanged, this, &ProfileViewModel::bestChanged);
    }
    emit gameChanged();
    emit bestChanged();
}

QString ProfileViewModel::best() const
{
    if (m_Game == nullptr)
    {
        return six_digits(0);
    }
    return six_digits(m_Best[profile::mode_index(to_sim(m_Game->mode()))]);
}

void ProfileViewModel::on_state_changed()
{
    if (m_Game->state() == GameViewModel::State::GameOver)
    {
        record_game(to_sim(m_Game->mode()), static_cast<std::uint32_t>(m_Game->points()));
    }
}

void ProfileViewModel::record_game(sim::Mode mode, std::uint32_t score)
{
    std::uint32_t& best = m_Best[profile::mode_index(mode)];

    m_NewBest = score > best;
    if (m_NewBest)
    {
        best = score;
        m_PointsToBest = 0;
    }
    else
    {
        m_PointsToBest = best == 0 ? -1 : static_cast<int>(best - score);
    }

    m_UnlockedNow.clear();
    const profile::Unlocks opened = m_Data.unlocks.record_score(mode, score);
    for (const auto unlockable : {profile::Unlockable::PORTALS, profile::Unlockable::HOT_POTATO})
    {
        if (opened.has(unlockable))
        {
            m_UnlockedNow.push_back(ModeListModel::name_of(entry_of(unlockable)));
        }
    }
    if (!opened.none())
    {
        m_Modes.refresh();
    }

    emit bestChanged();
    emit resultChanged();
}

} // namespace snake::ui
