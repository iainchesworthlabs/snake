// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/simulation.hpp"

namespace snake::sim
{

namespace
{

/// The cell one step from `from`, or nothing when that is off the board and the edges are walls.
std::optional<Cell> step(Cell from, Direction direction, EdgeRule edges) noexcept
{
    // Indexed by Direction: up, down, left, right.
    constexpr int DX[] = {0, 0, -1, 1};
    constexpr int DY[] = {-1, 1, 0, 0};
    const auto index = static_cast<std::size_t>(direction) % 4;
    int x = from.x + DX[index];
    int y = from.y + DY[index];
    if (edges == EdgeRule::WRAP)
    {
        x = (x + BOARD_COLUMNS) % BOARD_COLUMNS;
        y = (y + BOARD_ROWS) % BOARD_ROWS;
    }
    else if (x < 0 || x >= BOARD_COLUMNS || y < 0 || y >= BOARD_ROWS)
    {
        return std::nullopt;
    }
    return Cell{static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y)};
}

/// True when `to` is one step from `from` in some direction.
bool adjacent(Cell from, Cell to, EdgeRule edges) noexcept
{
    constexpr Direction ALL[] = {Direction::UP, Direction::DOWN, Direction::LEFT, Direction::RIGHT};
    for (const Direction direction : ALL)
    {
        const std::optional<Cell> next = step(from, direction, edges);
        if (next && *next == to)
        {
            return true;
        }
    }
    return false;
}

} // namespace

Simulation::Simulation(EdgeRule edges, std::uint64_t seed) noexcept : m_Edges(edges), m_Random(seed)
{
    // The head is just right of the middle, with the body trailing to the left.
    constexpr std::uint8_t HEAD_X = BOARD_COLUMNS / 2;
    constexpr std::uint8_t ROW = BOARD_ROWS / 2;
    for (std::size_t part = 0; part < INITIAL_LENGTH; ++part)
    {
        const Cell cell{static_cast<std::uint8_t>(HEAD_X - part), ROW};
        static_cast<void>(m_Snake.push_back(cell));
        m_Occupied.set(to_index(cell));
    }
    static_cast<void>(place_food());
}

std::optional<Simulation> Simulation::from_setup(const Setup& setup) noexcept
{
    if (setup.snake.empty() || setup.snake.size() > BOARD_CELLS || !on_board(setup.food))
    {
        return std::nullopt;
    }
    Simulation game;
    game.m_Edges = setup.edges;
    game.m_Heading = setup.heading;
    game.m_Random = Random(setup.seed);
    for (std::size_t part = 0; part < setup.snake.size(); ++part)
    {
        const Cell cell = setup.snake[part];
        if (!on_board(cell) || game.m_Occupied.test(to_index(cell)))
        {
            return std::nullopt;
        }
        if (part > 0 && !adjacent(setup.snake[part - 1], cell, setup.edges))
        {
            return std::nullopt;
        }
        static_cast<void>(game.m_Snake.push_back(cell));
        game.m_Occupied.set(to_index(cell));
    }
    // The head cannot point at the next part of the body.
    if (setup.snake.size() > 1)
    {
        const std::optional<Cell> ahead = step(setup.snake[0], setup.heading, setup.edges);
        if (ahead && *ahead == setup.snake[1])
        {
            return std::nullopt;
        }
    }
    if (game.m_Occupied.test(to_index(setup.food)) || !Scoring::valid(setup.scoring))
    {
        return std::nullopt;
    }
    if (setup.bonus)
    {
        const Bonus& bonus = *setup.bonus;
        if (!on_board(bonus.cell) || game.m_Occupied.test(to_index(bonus.cell)) || bonus.cell == setup.food ||
            bonus.ticks_left == 0 || bonus.ticks_left > BONUS_LIFETIME_TICKS)
        {
            return std::nullopt;
        }
    }
    game.m_Food = setup.food;
    game.m_Bonus = setup.bonus;
    game.m_Scoring = Scoring(setup.scoring);
    return game;
}

bool Simulation::turn(Direction input) noexcept
{
    return m_Input.push(m_Heading, input);
}

TickOutcome Simulation::tick() noexcept
{
    if (!m_Running)
    {
        return TickOutcome::NOT_RUNNING;
    }
    m_Scoring.advance();
    if (const std::optional<Direction> next_heading = m_Input.pop())
    {
        m_Heading = *next_heading;
    }

    const std::optional<Cell> next = step(m_Snake.front(), m_Heading, m_Edges);
    if (!next)
    {
        m_Running = false;
        return TickOutcome::HIT_WALL;
    }

    // The tail moves away this tick, so its cell is free, unless the snake eats and keeps its tail.
    const bool eating_food = *next == m_Food;
    const bool eating_bonus = m_Bonus && *next == m_Bonus->cell;
    const bool eating = eating_food || eating_bonus;
    const Cell tail = m_Snake.back();
    const bool tail_is_free = !eating && *next == tail;
    if (m_Occupied.test(to_index(*next)) && !tail_is_free)
    {
        m_Running = false;
        return TickOutcome::HIT_SELF;
    }

    if (!eating)
    {
        static_cast<void>(m_Snake.pop_back());
        m_Occupied.reset(to_index(tail));
    }
    static_cast<void>(m_Snake.push_front(*next));
    m_Occupied.set(to_index(*next));

    if (eating_bonus)
    {
        static_cast<void>(m_Scoring.eat_bonus());
        m_Bonus.reset();
        return TickOutcome::ATE_BONUS;
    }

    // An eating snake reaches the bonus item before it runs out. Otherwise it loses a tick.
    if (m_Bonus && --m_Bonus->ticks_left == 0)
    {
        m_Bonus.reset();
    }
    if (!eating_food)
    {
        return TickOutcome::MOVED;
    }

    const Scoring::FoodResult eaten = m_Scoring.eat_food();
    if (!place_food())
    {
        m_Running = false;
        m_Won = true;
        return TickOutcome::WON;
    }
    if (eaten.bonus_due)
    {
        place_bonus();
    }
    return TickOutcome::ATE;
}

bool Simulation::place_food() noexcept
{
    std::size_t empty_cells = BOARD_CELLS - m_Occupied.count();
    if (m_Bonus)
    {
        if (empty_cells == 1)
        {
            // The bonus item sits on the last empty cell. The food goes there instead.
            m_Bonus.reset();
        }
        else
        {
            m_Occupied.set(to_index(m_Bonus->cell));
            --empty_cells;
        }
    }
    if (empty_cells == 0)
    {
        return false;
    }
    const auto rank = static_cast<std::size_t>(m_Random.below(static_cast<std::uint32_t>(empty_cells)));
    m_Food = from_index(static_cast<std::uint16_t>(m_Occupied.nth_clear(rank)));
    if (m_Bonus)
    {
        m_Occupied.reset(to_index(m_Bonus->cell));
    }
    return true;
}

void Simulation::place_bonus() noexcept
{
    // The food is not in the occupancy set, so mark it to keep the bonus item off it.
    m_Occupied.set(to_index(m_Food));
    const std::size_t empty_cells = BOARD_CELLS - m_Occupied.count();
    if (empty_cells > 0)
    {
        const auto rank = static_cast<std::size_t>(m_Random.below(static_cast<std::uint32_t>(empty_cells)));
        m_Bonus = Bonus{.cell = from_index(static_cast<std::uint16_t>(m_Occupied.nth_clear(rank)))};
    }
    m_Occupied.reset(to_index(m_Food));
}

} // namespace snake::sim
