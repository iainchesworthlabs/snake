// SPDX-License-Identifier: GPL-3.0-or-later
#include "viewmodel/orientation_director.hpp"

#include "device/device.hpp"
#include "viewmodel/game_view_model.hpp"

#include <utility>

namespace snake::ui
{

OrientationDirector::OrientationDirector(QObject* parent) : OrientationDirector(&device::lock_orientation, parent) {}

OrientationDirector::OrientationDirector(Lock lock, QObject* parent) : QObject(parent), m_Lock(std::move(lock)) {}

OrientationDirector::~OrientationDirector()
{
    // The program is closing, or the director is let go of: the screen is not held on its account.
    if (m_Locked && m_Lock)
    {
        m_Lock(false);
    }
}

void OrientationDirector::setGame(GameViewModel* game)
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
        connect(m_Game, &GameViewModel::stateChanged, this, &OrientationDirector::update);
    }
    emit gameChanged();
    update();
}

void OrientationDirector::setActive(bool active)
{
    if (active == m_Active)
    {
        return;
    }
    m_Active = active;
    emit activeChanged();
    update();
}

void OrientationDirector::update()
{
    // A game is on from Play until the player leaves for the title: while it runs, while it is paused and at its end,
    // where Play again goes on with it.
    const bool wanted = m_Active && m_Game != nullptr && m_Game->state() != GameViewModel::State::NotStarted;
    if (wanted == m_Locked)
    {
        return;
    }
    m_Locked = wanted;
    if (m_Lock)
    {
        m_Lock(wanted);
    }
    emit lockedChanged();
}

} // namespace snake::ui
