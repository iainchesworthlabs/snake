// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/sim/cell.hpp"
#include "snake/sim/direction.hpp"
#include "snake/sim/edge_rule.hpp"
#include "snake/sim/input_buffer.hpp"
#include "snake/sim/occupancy_set.hpp"
#include "snake/sim/random.hpp"
#include "snake/sim/ring_buffer.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace snake::sim
{

/// What one call to tick() did.
enum class TickOutcome : std::uint8_t
{
    MOVED,       // the snake moved one cell
    ATE,         // the snake moved onto the food and grew
    HIT_WALL,    // the next cell is off the board and the edge rule is WALLS: game over
    HIT_SELF,    // the next cell is part of the snake's body: game over
    WON,         // the snake ate and no empty cell is left for new food: the game ends as a perfect game
    NOT_RUNNING, // the game had already ended, nothing changed
};

/// The rules of one game of Snake on the 20 by 20 board. This is the whole game state and every rule; there is no
/// clock, no allocation and no platform code in it. The same seed and the same inputs on each tick give the same
/// game on every platform.
class Simulation
{
public:
    static constexpr std::size_t INITIAL_LENGTH = 3;

    /// A new game: a snake of INITIAL_LENGTH in the middle of the board heading right, and the first food.
    Simulation(EdgeRule edges, std::uint64_t seed) noexcept;

    /// A game in a given position, for tests and for restoring a saved game. The snake is listed head first.
    struct Setup
    {
        EdgeRule edges = EdgeRule::WALLS;
        Direction heading = Direction::RIGHT;
        std::span<const Cell> snake;
        Cell food;
        std::uint64_t seed = 0;
    };

    /// Builds the game in `setup`, or returns nothing when the position is impossible: a cell off the board, a cell
    /// used twice, cells that do not form a connected snake, a heading that points into the snake's neck, or food
    /// on the snake.
    [[nodiscard]] static std::optional<Simulation> from_setup(const Setup& setup) noexcept;

    /// Queues a direction for the next ticks. Returns false when the input is dropped (see InputBuffer).
    bool turn(Direction input) noexcept;

    /// Advances the game by one tick. The order is: take the next queued direction; find the new head cell, with
    /// the edge rule; end the game on a wall; end it if the head lands on the body (the tail cell counts as free
    /// because it moves away, unless the snake is eating); add the head and, unless eating, remove the tail;
    /// when eating, place new food, and end the game as a win if there is no room for it.
    [[nodiscard]] TickOutcome tick() noexcept;

    [[nodiscard]] bool running() const noexcept
    {
        return m_Running;
    }

    /// True when the game ended because the snake filled the board.
    [[nodiscard]] bool won() const noexcept
    {
        return m_Won;
    }

    [[nodiscard]] EdgeRule edge_rule() const noexcept
    {
        return m_Edges;
    }

    /// The direction the snake travels now. Queued turns are not included.
    [[nodiscard]] Direction heading() const noexcept
    {
        return m_Heading;
    }

    /// The snake, head first.
    [[nodiscard]] const RingBuffer<Cell, BOARD_CELLS>& snake() const noexcept
    {
        return m_Snake;
    }

    [[nodiscard]] Cell head() const noexcept
    {
        return m_Snake.front();
    }

    [[nodiscard]] std::size_t length() const noexcept
    {
        return m_Snake.size();
    }

    /// The food. Meaningless after a win, when there is no room for it.
    [[nodiscard]] Cell food() const noexcept
    {
        return m_Food;
    }

private:
    Simulation() noexcept = default;

    /// Puts the food on an empty cell chosen uniformly with the game's generator. False when no cell is empty.
    [[nodiscard]] bool place_food() noexcept;

    EdgeRule m_Edges = EdgeRule::WALLS;
    Direction m_Heading = Direction::RIGHT;
    RingBuffer<Cell, BOARD_CELLS> m_Snake;
    OccupancySet<BOARD_CELLS> m_Occupied;
    Cell m_Food;
    InputBuffer m_Input;
    Random m_Random{0};
    bool m_Running = true;
    bool m_Won = false;
};

} // namespace snake::sim
