// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/sim/direction.hpp"
#include "snake/sim/mode.hpp"
#include "snake/sim/simulation.hpp"

#include <cstdint>
#include <optional>

namespace snake::sim
{

/// Where a game is in its life. The screen flow in the requirements shows the same four states.
enum class GameState : std::uint8_t
{
    NOT_STARTED, // the board is set up and waiting for the player; ticks and turns do nothing
    RUNNING,
    PAUSED, // by the player, or because the app lost focus, went to the background or the device slept
    GAME_OVER,
};

/// One game in one mode: the simulation, the state it is in and the speed it runs at. The application drives it
/// with a clock: it calls tick() every tick_interval_ms() while the state is RUNNING.
///
/// Turns are accepted only while the game runs, so a press during a pause or before the start never sits in the
/// input buffer waiting to steer the first tick after the game resumes. A restart is a new Session.
class Session
{
public:
    Session(Mode mode, std::uint64_t seed) noexcept;

    /// A session for a game in progress, to restore a saved game and to set up tests. Returns nothing when the
    /// simulation's edge rule is not the mode's, or when `state` does not fit the simulation: GAME_OVER exactly
    /// when the simulation has ended.
    [[nodiscard]] static std::optional<Session> from_simulation(Mode mode, const Simulation& simulation,
                                                                GameState state) noexcept;

    /// NOT_STARTED to RUNNING. Returns false, changing nothing, in any other state.
    bool start() noexcept;

    /// RUNNING to PAUSED. Returns false, changing nothing, in any other state.
    bool pause() noexcept;

    /// PAUSED to RUNNING. Returns false, changing nothing, in any other state.
    bool resume() noexcept;

    /// Queues a direction. Returns false when the game is not running or the input is dropped (see InputBuffer).
    bool turn(Direction input) noexcept;

    /// One tick of the game, when it is running. In any other state nothing changes and the result is
    /// NOT_RUNNING. A tick that ends the game moves the session to GAME_OVER.
    [[nodiscard]] TickOutcome tick() noexcept;

    /// The time to wait before the next tick, from the mode and the foods eaten so far.
    [[nodiscard]] std::uint16_t tick_interval_ms() const noexcept
    {
        return snake::sim::tick_interval_ms(m_Mode, m_Simulation.foods_eaten());
    }

    [[nodiscard]] GameState state() const noexcept
    {
        return m_State;
    }

    [[nodiscard]] Mode mode() const noexcept
    {
        return m_Mode;
    }

    [[nodiscard]] const Simulation& simulation() const noexcept
    {
        return m_Simulation;
    }

private:
    Session(Mode mode, const Simulation& simulation, GameState state) noexcept;

    Mode m_Mode;
    Simulation m_Simulation;
    GameState m_State = GameState::NOT_STARTED;
};

} // namespace snake::sim
