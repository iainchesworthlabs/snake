// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/sim/cell.hpp"
#include "snake/sim/simulation.hpp"

#include <cstdint>

namespace snake::sim
{

/// How a game ended, as far as a Tally saw it.
enum class Ending : std::uint8_t
{
    NONE,       // the game has not ended, as far as the tally has seen
    WALL,       // the snake hit the wall
    SELF,       // the snake hit itself
    BOMB,       // the bomb went off
    FULL_BOARD, // the snake filled the board
};

/// What happened in one game, counted for whoever drives the game: how much was eaten, how often the snake wrapped,
/// went through a portal or shed, how the game ended. It is not part of the Simulation, the snapshot or the state
/// hash: the driver feeds it the outcome of each tick and the Simulation after it. Counters stop at their largest
/// value instead of wrapping round.
class Tally
{
public:
    /// An empty tally for a game that has not started. Call start() for a real game.
    Tally() noexcept = default;

    /// Clears everything for a new game whose snake's head is on `head`.
    void start(Cell head) noexcept;

    /// Counts one tick: `outcome` is what tick() returned and `game` is the Simulation after it. A NOT_RUNNING tick
    /// changes nothing.
    void observe(TickOutcome outcome, const Simulation& game) noexcept;

    /// Counts one shed that took effect, that is, one for which Simulation::shed() returned true.
    void note_shed() noexcept;

    /// Normal foods eaten, counting the last food that won the game.
    [[nodiscard]] std::uint16_t blueberries() const noexcept
    {
        return m_Blueberries;
    }

    [[nodiscard]] std::uint16_t bonus_items() const noexcept
    {
        return m_BonusItems;
    }

    [[nodiscard]] std::uint16_t bombs_eaten() const noexcept
    {
        return m_BombsEaten;
    }

    [[nodiscard]] std::uint16_t sheds() const noexcept
    {
        return m_Sheds;
    }

    [[nodiscard]] std::uint16_t portals_used() const noexcept
    {
        return m_PortalsUsed;
    }

    /// The largest combo the snake had straight after eating a normal food, or 0 when it ate none.
    [[nodiscard]] std::uint8_t top_combo() const noexcept
    {
        return m_TopCombo;
    }

    /// Foods of every kind eaten (blueberries, bonus items and bombs), as the speed curve counts them, as of the last
    /// tick seen.
    [[nodiscard]] std::uint16_t foods_eaten() const noexcept
    {
        return m_FoodsEaten;
    }

    /// True once the head has gone over the edge of a board that wraps and come out on the other side. A portal does
    /// not count, and nor does a tick that ended the game, because the snake does not move on it.
    [[nodiscard]] bool wrapped() const noexcept
    {
        return m_Wrapped;
    }

    [[nodiscard]] Ending ended_by() const noexcept
    {
        return m_EndedBy;
    }

    friend constexpr bool operator==(const Tally& lhs, const Tally& rhs) noexcept = default;

private:
    std::uint16_t m_Blueberries = 0;
    std::uint16_t m_BonusItems = 0;
    std::uint16_t m_BombsEaten = 0;
    std::uint16_t m_Sheds = 0;
    std::uint16_t m_PortalsUsed = 0;
    std::uint16_t m_FoodsEaten = 0;
    std::uint8_t m_TopCombo = 0;
    bool m_Wrapped = false;
    Ending m_EndedBy = Ending::NONE;
    Cell m_Head; // where the head was after the last tick that moved it
};

} // namespace snake::sim
