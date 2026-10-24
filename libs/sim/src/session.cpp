// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/session.hpp"

namespace snake::sim
{

Session::Session(Mode mode, std::uint64_t seed) noexcept : m_Mode(mode), m_Simulation(edge_rule_of(mode), seed) {}

Session::Session(Mode mode, const Simulation& simulation, GameState state) noexcept
    : m_Mode(mode), m_Simulation(simulation), m_State(state)
{
}

std::optional<Session> Session::from_simulation(Mode mode, const Simulation& simulation, GameState state) noexcept
{
    if (simulation.edge_rule() != edge_rule_of(mode) || (state == GameState::GAME_OVER) == simulation.running())
    {
        return std::nullopt;
    }
    return Session(mode, simulation, state);
}

bool Session::start() noexcept
{
    if (m_State != GameState::NOT_STARTED)
    {
        return false;
    }
    m_State = GameState::RUNNING;
    return true;
}

bool Session::pause() noexcept
{
    if (m_State != GameState::RUNNING)
    {
        return false;
    }
    m_State = GameState::PAUSED;
    return true;
}

bool Session::resume() noexcept
{
    if (m_State != GameState::PAUSED)
    {
        return false;
    }
    m_State = GameState::RUNNING;
    return true;
}

bool Session::turn(Direction input) noexcept
{
    return m_State == GameState::RUNNING && m_Simulation.turn(input);
}

TickOutcome Session::tick() noexcept
{
    if (m_State != GameState::RUNNING)
    {
        return TickOutcome::NOT_RUNNING;
    }
    const TickOutcome outcome = m_Simulation.tick();
    if (!m_Simulation.running())
    {
        m_State = GameState::GAME_OVER;
    }
    return outcome;
}

} // namespace snake::sim
