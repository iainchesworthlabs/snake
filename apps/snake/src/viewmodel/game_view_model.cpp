// SPDX-License-Identifier: GPL-3.0-or-later
#include "viewmodel/game_view_model.hpp"

#include "input/qt_keys.hpp"
#include "snake/input/action.hpp"
#include "snake/input/key_map.hpp"
#include "snake/sim/mode.hpp"
#include "snake/sim/scoring.hpp"
#include "snake/sim/session.hpp"

#include <QRandomGenerator>
#include <QString>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace snake::ui
{

namespace
{

constexpr sim::Mode to_sim(GameViewModel::Mode mode) noexcept
{
    return mode == GameViewModel::Mode::Easy ? sim::Mode::EASY : sim::Mode::CLASSIC;
}

constexpr GameViewModel::Mode from_sim(sim::Mode mode) noexcept
{
    return mode == sim::Mode::EASY ? GameViewModel::Mode::Easy : GameViewModel::Mode::Classic;
}

constexpr GameViewModel::State from_sim(sim::GameState state) noexcept
{
    switch (state)
    {
    case sim::GameState::NOT_STARTED:
        return GameViewModel::State::NotStarted;
    case sim::GameState::RUNNING:
        return GameViewModel::State::Running;
    case sim::GameState::PAUSED:
        return GameViewModel::State::Paused;
    case sim::GameState::GAME_OVER:
        break;
    }
    return GameViewModel::State::GameOver;
}

std::uint64_t system_seed()
{
    return QRandomGenerator::system()->generate64();
}

} // namespace

GameViewModel::GameViewModel(QObject* parent) : GameViewModel(&system_seed, parent) {}

GameViewModel::GameViewModel(SeedSource seeds, QObject* parent)
    : QObject(parent), m_Seeds(std::move(seeds)), m_Session(sim::Mode::CLASSIC, m_Seeds())
{
}

GameViewModel::State GameViewModel::state() const
{
    return from_sim(m_Session.state());
}

QString GameViewModel::score() const
{
    const auto digits = sim::score_digits(m_Session.simulation().score());
    return QString::fromLatin1(digits.data(), static_cast<qsizetype>(digits.size()));
}

GameViewModel::Mode GameViewModel::mode() const
{
    return from_sim(m_Session.mode());
}

double GameViewModel::tickProgress() const
{
    const double interval_us = static_cast<double>(m_Session.tick_interval_ms()) * 1000.0;
    return std::clamp(static_cast<double>(m_Pacer.pending_us()) / interval_us, 0.0, 1.0);
}

void GameViewModel::setMode(Mode mode)
{
    if (mode == this->mode())
    {
        return;
    }
    const sim::GameState state_before = m_Session.state();
    const std::uint32_t score_before = m_Session.simulation().score();
    m_Session = sim::Session(to_sim(mode), m_Seeds());
    m_Pacer.reset();
    emit modeChanged();
    publish(state_before, score_before, true);
}

void GameViewModel::newGame()
{
    const sim::GameState state_before = m_Session.state();
    const std::uint32_t score_before = m_Session.simulation().score();
    m_Session = sim::Session(m_Session.mode(), m_Seeds());
    m_Pacer.reset();
    publish(state_before, score_before, true);
}

void GameViewModel::start()
{
    const sim::GameState state_before = m_Session.state();
    if (m_Session.start())
    {
        m_Pacer.reset();
        publish(state_before, m_Session.simulation().score(), false);
    }
}

void GameViewModel::pause()
{
    const sim::GameState state_before = m_Session.state();
    if (m_Session.pause())
    {
        publish(state_before, m_Session.simulation().score(), false);
    }
}

void GameViewModel::resume()
{
    const sim::GameState state_before = m_Session.state();
    if (m_Session.resume())
    {
        // The time of the pause is not the game's: the pacer collects none while the game is paused, and what it held
        // when the game stopped is kept, so that the snake carries on from where the picture showed it.
        publish(state_before, m_Session.simulation().score(), false);
    }
}

void GameViewModel::advance(double seconds)
{
    if (m_Session.state() != sim::GameState::RUNNING || !(seconds > 0.0))
    {
        return;
    }
    constexpr double MAX_US = std::numeric_limits<std::uint32_t>::max();
    m_Pacer.add(static_cast<std::uint32_t>(std::llround(std::min(seconds * 1.0e6, MAX_US))));

    const sim::GameState state_before = m_Session.state();
    const std::uint32_t score_before = m_Session.simulation().score();
    bool ticked = false;
    while (m_Session.state() == sim::GameState::RUNNING && m_Pacer.due(m_Session.tick_interval_ms()))
    {
        static_cast<void>(m_Session.tick());
        ticked = true;
    }
    if (ticked)
    {
        publish(state_before, score_before, true);
    }
    emit progressChanged();
}

bool GameViewModel::handleKey(int qtKey, bool autoRepeat)
{
    const auto key = key_from_qt(qtKey);
    if (!key)
    {
        return false;
    }
    if (autoRepeat)
    {
        return true;
    }
    if (const auto direction = input::steer_for(*key))
    {
        static_cast<void>(m_Session.turn(*direction));
    }
    else if (const auto action = input::action_for(*key))
    {
        perform(*action);
    }
    return true;
}

void GameViewModel::perform(input::Action action)
{
    const sim::GameState state = m_Session.state();
    switch (action)
    {
    case input::Action::PAUSE_RESUME:
        if (state == sim::GameState::RUNNING)
        {
            pause();
        }
        else
        {
            resume();
        }
        break;
    case input::Action::MENU:
        // There is no menu yet; it pauses, as the menu will.
        pause();
        break;
    case input::Action::CONFIRM:
        switch (state)
        {
        case sim::GameState::GAME_OVER:
            newGame();
            start();
            break;
        case sim::GameState::PAUSED:
            resume();
            break;
        case sim::GameState::NOT_STARTED:
        case sim::GameState::RUNNING:
            start();
            break;
        }
        break;
    }
}

void GameViewModel::publish(sim::GameState state_before, std::uint32_t score_before, bool board_changed)
{
    if (m_Session.state() != state_before)
    {
        emit stateChanged();
    }
    if (m_Session.simulation().score() != score_before)
    {
        emit scoreChanged();
    }
    if (board_changed)
    {
        emit boardChanged();
    }
}

} // namespace snake::ui
