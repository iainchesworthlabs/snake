// SPDX-License-Identifier: GPL-3.0-or-later
#include "viewmodel/haptics_director.hpp"

#include "device/haptics.hpp"
#include "viewmodel/game_view_model.hpp"

namespace snake::ui
{

HapticsDirector::HapticsDirector(QObject* parent) : HapticsDirector(device::default_haptics(), parent) {}

HapticsDirector::HapticsDirector(device::Haptics* haptics, QObject* parent) : QObject(parent), m_Haptics(haptics) {}

void HapticsDirector::setGame(GameViewModel* game)
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
        connect(m_Game, &GameViewModel::tickEvent, this, &HapticsDirector::on_tick_event);
    }
    emit gameChanged();
}

void HapticsDirector::on_tick_event(GameViewModel::Event event)
{
    if (m_Haptics == nullptr)
    {
        return;
    }
    switch (event)
    {
    case GameViewModel::Event::Ate:
    case GameViewModel::Event::AteBonus:
    case GameViewModel::Event::Won:
        m_Haptics->tap();
        break;
    case GameViewModel::Event::Crashed:
        m_Haptics->thud();
        break;
    }
}

} // namespace snake::ui
