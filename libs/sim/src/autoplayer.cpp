// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/autoplayer.hpp"

#include "snake/sim/cell.hpp"
#include "snake/sim/direction.hpp"
#include "snake/sim/movement.hpp"
#include "snake/sim/occupancy_set.hpp"
#include "snake/sim/simulation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace snake::sim
{

namespace
{

constexpr std::array<Direction, 4> ORDER{Direction::UP, Direction::DOWN, Direction::LEFT, Direction::RIGHT};

/// For every cell, how many ticks until the snake has left it: a body cell `i` places from the head is free after
/// length - i ticks, and a cell with no body on it is free now (0).
using FreeAfter = std::array<std::uint16_t, BOARD_CELLS>;

/// The index of a cell on the board. The remainder changes nothing for a cell on the board, and it lets the compiler
/// see that the index is inside the arrays below.
std::uint16_t index_of(Cell cell) noexcept
{
    return static_cast<std::uint16_t>(to_index(cell) % BOARD_CELLS);
}

FreeAfter free_after(const Simulation& game) noexcept
{
    FreeAfter times{};
    const std::size_t length = game.length();
    for (std::size_t part = 0; part < length; ++part)
    {
        times[index_of(game.snake()[part])] = static_cast<std::uint16_t>(length - part);
    }
    return times;
}

/// The first direction of a shortest way from the head to the food, or nothing when there is none. A step lands on
/// a cell at tick t only when the body has left that cell by then.
std::optional<Direction> first_step_to_food(const Simulation& game, const FreeAfter& times) noexcept
{
    const std::uint16_t start = index_of(game.head());
    const std::uint16_t goal = index_of(game.food());
    std::array<std::uint16_t, BOARD_CELLS> arrival{};
    std::array<Direction, BOARD_CELLS> first{};
    std::array<std::uint16_t, BOARD_CELLS> queue{};
    OccupancySet<BOARD_CELLS> seen;
    std::size_t next = 0;
    std::size_t end = 0;
    queue[end++] = start;
    seen.set(start);
    while (next < end)
    {
        const std::uint16_t current = queue[next++];
        for (const Direction direction : ORDER)
        {
            const std::optional<Cell> cell = step(from_index(current), direction, game.edge_rule());
            if (!cell)
            {
                continue;
            }
            const std::uint16_t index = index_of(*cell);
            const auto tick = static_cast<std::uint16_t>(arrival[current] + 1);
            if (seen.test(index) || times[index] > tick)
            {
                continue;
            }
            seen.set(index);
            arrival[index] = tick;
            first[index] = current == start ? direction : first[current];
            if (index == goal)
            {
                return first[index];
            }
            queue[end++] = index;
        }
    }
    return std::nullopt;
}

/// How many cells the snake could reach from `from` if it stood still, not counting cells the body covers after the
/// next tick.
std::size_t room_from(const Simulation& game, const FreeAfter& times, Cell from) noexcept
{
    std::array<std::uint16_t, BOARD_CELLS> queue{};
    OccupancySet<BOARD_CELLS> seen;
    std::size_t next = 0;
    std::size_t end = 0;
    queue[end++] = index_of(from);
    seen.set(queue[0]);
    while (next < end)
    {
        const std::uint16_t current = queue[next++];
        for (const Direction direction : ORDER)
        {
            const std::optional<Cell> cell = step(from_index(current), direction, game.edge_rule());
            if (!cell)
            {
                continue;
            }
            const std::uint16_t index = index_of(*cell);
            if (seen.test(index) || times[index] > 1)
            {
                continue;
            }
            seen.set(index);
            queue[end++] = index;
        }
    }
    return end;
}

} // namespace

Direction autoplay(const Simulation& game) noexcept
{
    const FreeAfter times = free_after(game);

    if (const std::optional<Direction> toward_food = first_step_to_food(game, times))
    {
        const Cell cell = step(game.head(), *toward_food, game.edge_rule()).value_or(game.head());
        if (room_from(game, times, cell) >= game.length())
        {
            return *toward_food;
        }
    }

    Direction best = game.heading();
    std::size_t best_room = 0;
    for (const Direction direction : ORDER)
    {
        const std::optional<Cell> cell = step(game.head(), direction, game.edge_rule());
        if (!cell || times[index_of(*cell)] > 1)
        {
            continue;
        }
        const std::size_t room = room_from(game, times, *cell);
        if (room > best_room)
        {
            best = direction;
            best_room = room;
        }
    }
    return best;
}

} // namespace snake::sim
