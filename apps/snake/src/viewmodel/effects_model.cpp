// SPDX-License-Identifier: GPL-3.0-or-later
#include "viewmodel/effects_model.hpp"

#include "viewmodel/game_view_model.hpp"

#include <cstdint>

namespace snake::ui
{

EffectsModel::EffectsModel(QObject* parent) : QObject(parent) {}

void EffectsModel::setGame(GameViewModel* game)
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
    m_Effects.clear();
    m_LastPoints = 0;
    if (m_Game != nullptr)
    {
        connect(m_Game, &GameViewModel::tickEvent, this, &EffectsModel::on_tick_event);
        connect(m_Game, &GameViewModel::stateChanged, this, &EffectsModel::on_state_changed);
        m_LastPoints = m_Game->points();
    }
    update_active();
    emit changed();
    emit gameChanged();
}

void EffectsModel::setReducedMotion(bool reduced)
{
    if (reduced == m_Effects.reduced_motion())
    {
        return;
    }
    m_Effects.set_reduced_motion(reduced);
    update_active();
    emit changed();
    emit reducedMotionChanged();
}

void EffectsModel::on_tick_event(GameViewModel::Event event)
{
    const std::int64_t points = m_Game->points();
    const auto worth = static_cast<std::uint32_t>(points > m_LastPoints ? points - m_LastPoints : 0);
    m_LastPoints = points;
    switch (event)
    {
    case GameViewModel::Event::Ate:
    case GameViewModel::Event::Won:
        m_Effects.eaten(m_Game->session().simulation().head(), worth, false);
        break;
    case GameViewModel::Event::AteBonus:
        m_Effects.eaten(m_Game->session().simulation().head(), worth, true);
        break;
    case GameViewModel::Event::Crashed:
        m_Effects.crashed();
        break;
    }
    update_active();
    emit changed();
}

void EffectsModel::on_state_changed()
{
    // A game that begins from the title or from the end of another starts with a clean board. A pause and its resume
    // leave everything where it was.
    const bool fresh = m_Game->state() == GameViewModel::State::Running && m_Game->session().simulation().ticks() == 0;
    if (m_Game->state() == GameViewModel::State::NotStarted || fresh)
    {
        m_Effects.clear();
    }
    m_LastPoints = m_Game->points();
    update_active();
    emit changed();
}

void EffectsModel::advance(double seconds)
{
    m_Effects.advance(seconds);
    update_active();
    emit changed();
}

void EffectsModel::update_active()
{
    const bool active = m_Effects.active() && (m_Game == nullptr || m_Game->state() != GameViewModel::State::Paused);
    if (active != m_Active)
    {
        m_Active = active;
        emit activeChanged();
    }
}

} // namespace snake::ui
