// SPDX-License-Identifier: GPL-3.0-or-later
#include "viewmodel/game_view_model.hpp"

#include "snake/timing_loop.hpp"

#include <iostream>
#include <utility>

namespace snake::ui
{

GameViewModel::GameViewModel(QObject* parent)
    : GameViewModel(std::make_shared<const StreamLogger>(std::clog), TimingLoop::frame_interval(), parent)
{
}

GameViewModel::GameViewModel(std::shared_ptr<const Logger> logger, std::chrono::milliseconds interval, QObject* parent)
    : QObject(parent), m_Game(std::move(logger))
{
    m_Timer.setTimerType(Qt::PreciseTimer);
    m_Timer.setInterval(interval);
    connect(&m_Timer, &QTimer::timeout, this, [this] { tick(); });
}

qint64 GameViewModel::frame() const
{
    return static_cast<qint64>(m_Game.frame_count());
}

QString GameViewModel::state() const
{
    const auto name = to_string_view(m_Game.state());
    return QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()));
}

void GameViewModel::start()
{
    if (m_Game.start())
    {
        m_Timer.start();
        emit stateChanged();
    }
}

void GameViewModel::pause()
{
    if (m_Game.pause())
    {
        m_Timer.stop();
        emit stateChanged();
    }
}

void GameViewModel::resume()
{
    if (m_Game.resume())
    {
        m_Timer.start();
        emit stateChanged();
    }
}

void GameViewModel::end()
{
    if (m_Game.end())
    {
        m_Timer.stop();
        emit stateChanged();
    }
}

void GameViewModel::tick()
{
    if (m_Game.tick())
    {
        emit frameChanged();
    }
}

} // namespace snake::ui
