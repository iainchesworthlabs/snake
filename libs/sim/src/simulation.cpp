// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/simulation.hpp"

#include "snake/sim/movement.hpp"

namespace snake::sim
{

namespace
{

constexpr Direction ALL_DIRECTIONS[] = {Direction::UP, Direction::DOWN, Direction::LEFT, Direction::RIGHT};

/// True when `to` is one step from `from` in some direction.
bool adjacent(Cell from, Cell to, EdgeRule edges) noexcept
{
    for (const Direction direction : ALL_DIRECTIONS)
    {
        const std::optional<Cell> next = step(from, direction, edges);
        if (next && *next == to)
        {
            return true;
        }
    }
    return false;
}

/// True when a head that stepped from `from` onto a portal came out on `to`.
bool through_portal(Cell from, Cell to, EdgeRule edges, const std::optional<Simulation::PortalCells>& portals) noexcept
{
    if (!portals)
    {
        return false;
    }
    for (const Direction direction : ALL_DIRECTIONS)
    {
        const std::optional<Cell> next = step(from, direction, edges);
        if (next && portals->exit_from(*next) == to)
        {
            return true;
        }
    }
    return false;
}

} // namespace

Simulation::Simulation(EdgeRule edges, std::uint64_t seed, Variant variant) noexcept
    : m_Edges(edges), m_Variant(variant), m_Random(seed)
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
    if (m_Variant == Variant::PORTALS)
    {
        place_portals();
    }
    static_cast<void>(place_food());
}

std::optional<Simulation> Simulation::from_setup(const Setup& setup) noexcept
{
    if (setup.snake.empty() || setup.snake.size() > BOARD_CELLS || !on_board(setup.food) ||
        (setup.random && (setup.random->increment & 1U) == 0) || (setup.won && setup.running) ||
        (setup.won && setup.snake.size() != BOARD_CELLS))
    {
        return std::nullopt;
    }
    // Each variant has its own pieces, and no other has them.
    const bool portal_game = setup.variant == Variant::PORTALS;
    const bool potato_game = setup.variant == Variant::HOT_POTATO;
    if (portal_game != setup.portals.has_value() || (!potato_game && (setup.potato || setup.lump)) ||
        (!potato_game && setup.normal_since_potato != 0) || setup.normal_since_potato > POTATO_EVERY ||
        (setup.potato && setup.lump))
    {
        return std::nullopt;
    }
    if (setup.portals)
    {
        const auto& cells = setup.portals->cells;
        for (std::size_t first = 0; first < PORTAL_COUNT; ++first)
        {
            if (!on_board(cells[first]) || cells[first] == setup.food)
            {
                return std::nullopt;
            }
            for (std::size_t second = first + 1; second < PORTAL_COUNT; ++second)
            {
                if (cells[first] == cells[second])
                {
                    return std::nullopt;
                }
            }
        }
    }
    Simulation game;
    game.m_Edges = setup.edges;
    game.m_Heading = setup.heading;
    game.m_Variant = setup.variant;
    game.m_Portals = setup.portals;
    game.m_Random = setup.random ? Random(*setup.random) : Random(setup.seed);
    for (std::size_t part = 0; part < setup.snake.size(); ++part)
    {
        const Cell cell = setup.snake[part];
        if (!on_board(cell) || game.m_Occupied.test(to_index(cell)))
        {
            return std::nullopt;
        }
        // A part is one step from the part before it, or a portal away: the head came out of the other portal of a
        // pair.
        if (part > 0 && !adjacent(setup.snake[part - 1], cell, setup.edges) &&
            !through_portal(cell, setup.snake[part - 1], setup.edges, game.m_Portals))
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
    // After a win the food is the cell that was just eaten, so it is on the snake.
    if ((game.m_Occupied.test(to_index(setup.food)) && !setup.won) || !Scoring::valid(setup.scoring))
    {
        return std::nullopt;
    }
    if (setup.bonus)
    {
        const Bonus& bonus = *setup.bonus;
        if (!on_board(bonus.cell) || game.m_Occupied.test(to_index(bonus.cell)) || bonus.cell == setup.food ||
            (game.m_Portals && game.m_Portals->contains(bonus.cell)) || bonus.ticks_left == 0 ||
            bonus.ticks_left > BONUS_LIFETIME_TICKS)
        {
            return std::nullopt;
        }
    }
    if (setup.potato)
    {
        const Potato& potato = *setup.potato;
        if (!on_board(potato.cell) || game.m_Occupied.test(to_index(potato.cell)) || potato.cell == setup.food ||
            (setup.bonus && potato.cell == setup.bonus->cell) || potato.ticks_left == 0 ||
            potato.ticks_left > POTATO_LIFETIME_TICKS)
        {
            return std::nullopt;
        }
    }
    // The bomb that was swallowed is in a part of the body.
    if (setup.lump && !game.m_Occupied.test(to_index(*setup.lump)))
    {
        return std::nullopt;
    }
    game.m_Food = setup.food;
    game.m_Bonus = setup.bonus;
    game.m_Potato = setup.potato;
    game.m_Lump = setup.lump;
    game.m_NormalSincePotato = setup.normal_since_potato;
    game.m_Scoring = Scoring(setup.scoring);
    game.m_Ticks = setup.ticks;
    game.m_Running = setup.running;
    game.m_Won = setup.won;
    for (const Direction turn : setup.pending)
    {
        if (!game.m_Input.push(setup.heading, turn))
        {
            return std::nullopt;
        }
    }
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
    ++m_Ticks;
    m_Scoring.advance();
    if (const std::optional<Direction> next_heading = m_Input.pop())
    {
        m_Heading = *next_heading;
    }

    const std::optional<Cell> ahead = step(m_Snake.front(), m_Heading, m_Edges);
    if (!ahead)
    {
        m_Running = false;
        return TickOutcome::HIT_WALL;
    }

    // A head that steps onto a portal comes out of the other portal of its pair, and that is the cell it lands on.
    // Food, the bonus item and the bomb food are never on a portal, so a head that went through one eats nothing.
    Cell next = *ahead;
    bool teleported = false;
    if (m_Portals)
    {
        if (const std::optional<Cell> exit = m_Portals->exit_from(next))
        {
            next = *exit;
            teleported = true;
        }
    }

    // The tail moves away this tick, so its cell is free, unless the snake eats and keeps its tail.
    const bool eating_food = next == m_Food;
    const bool eating_bonus = m_Bonus && next == m_Bonus->cell;
    const bool eating_potato = m_Potato && next == m_Potato->cell;
    const bool eating = eating_food || eating_bonus || eating_potato;
    const Cell tail = m_Snake.back();
    const bool tail_is_free = !eating && next == tail;
    if (m_Occupied.test(to_index(next)) && !tail_is_free)
    {
        m_Running = false;
        return TickOutcome::HIT_SELF;
    }
    // The bomb goes off when the tail, which holds it, is about to move away.
    if (m_Lump && !eating && tail == *m_Lump)
    {
        m_Running = false;
        return TickOutcome::EXPLODED;
    }

    if (!eating)
    {
        static_cast<void>(m_Snake.pop_back());
        m_Occupied.reset(to_index(tail));
    }
    static_cast<void>(m_Snake.push_front(next));
    m_Occupied.set(to_index(next));

    if (eating_bonus)
    {
        static_cast<void>(m_Scoring.eat_bonus());
        m_Bonus.reset();
        return TickOutcome::ATE_BONUS;
    }

    // An eating snake reaches the bonus item and the bomb food before they run out. Otherwise they lose a tick.
    if (m_Bonus && --m_Bonus->ticks_left == 0)
    {
        m_Bonus.reset();
    }
    if (m_Potato && !eating_potato && --m_Potato->ticks_left == 0)
    {
        m_Potato.reset();
    }
    if (eating_potato)
    {
        static_cast<void>(m_Scoring.eat_potato());
        m_Potato.reset();
        m_Lump = next;
        return TickOutcome::ATE_POTATO;
    }
    if (!eating_food)
    {
        return teleported ? TickOutcome::TELEPORTED : TickOutcome::MOVED;
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
    if (m_Variant == Variant::HOT_POTATO)
    {
        if (m_NormalSincePotato < POTATO_EVERY)
        {
            ++m_NormalSincePotato;
        }
        // A bomb food waits for the snake to be long enough to have a body to roll down, and for the last one to be
        // gone.
        if (m_NormalSincePotato == POTATO_EVERY && !m_Potato && !m_Lump && m_Snake.size() >= POTATO_MIN_LENGTH)
        {
            place_potato();
        }
    }
    return TickOutcome::ATE;
}

std::optional<std::size_t> Simulation::lump_index() const noexcept
{
    if (!m_Lump)
    {
        return std::nullopt;
    }
    for (std::size_t part = 0; part < m_Snake.size(); ++part)
    {
        if (m_Snake[part] == *m_Lump)
        {
            return part;
        }
    }
    return std::nullopt;
}

bool Simulation::shed() noexcept
{
    if (!m_Running)
    {
        return false;
    }
    const std::optional<std::size_t> index = lump_index();
    if (!index || *index == 0)
    {
        return false;
    }
    while (m_Snake.size() > *index)
    {
        m_Occupied.reset(to_index(m_Snake.back()));
        static_cast<void>(m_Snake.pop_back());
    }
    m_Lump.reset();
    return true;
}

OccupancySet<BOARD_CELLS> Simulation::blocked_cells() const noexcept
{
    OccupancySet<BOARD_CELLS> blocked = m_Occupied;
    if (m_Portals)
    {
        for (const Cell cell : m_Portals->cells)
        {
            blocked.set(to_index(cell));
        }
    }
    if (m_Potato)
    {
        blocked.set(to_index(m_Potato->cell));
    }
    return blocked;
}

bool Simulation::place_food() noexcept
{
    OccupancySet<BOARD_CELLS> blocked = blocked_cells();
    std::size_t empty_cells = BOARD_CELLS - blocked.count();
    if (m_Bonus)
    {
        if (empty_cells == 1)
        {
            // The bonus item sits on the last empty cell. The food goes there instead.
            m_Bonus.reset();
        }
        else
        {
            blocked.set(to_index(m_Bonus->cell));
            --empty_cells;
        }
    }
    if (empty_cells == 0)
    {
        return false;
    }
    const auto rank = static_cast<std::size_t>(m_Random.below(static_cast<std::uint32_t>(empty_cells)));
    m_Food = from_index(static_cast<std::uint16_t>(blocked.nth_clear(rank)));
    return true;
}

void Simulation::place_bonus() noexcept
{
    // The food is not in the occupancy set, so mark it to keep the bonus item off it.
    OccupancySet<BOARD_CELLS> blocked = blocked_cells();
    blocked.set(to_index(m_Food));
    const std::size_t empty_cells = BOARD_CELLS - blocked.count();
    if (empty_cells > 0)
    {
        const auto rank = static_cast<std::size_t>(m_Random.below(static_cast<std::uint32_t>(empty_cells)));
        m_Bonus = Bonus{.cell = from_index(static_cast<std::uint16_t>(blocked.nth_clear(rank)))};
    }
}

void Simulation::place_potato() noexcept
{
    OccupancySet<BOARD_CELLS> blocked = blocked_cells();
    blocked.set(to_index(m_Food));
    if (m_Bonus)
    {
        blocked.set(to_index(m_Bonus->cell));
    }
    const std::size_t empty_cells = BOARD_CELLS - blocked.count();
    if (empty_cells > 0)
    {
        const auto rank = static_cast<std::size_t>(m_Random.below(static_cast<std::uint32_t>(empty_cells)));
        m_Potato = Potato{.cell = from_index(static_cast<std::uint16_t>(blocked.nth_clear(rank)))};
        m_NormalSincePotato = 0;
    }
}

void Simulation::place_portals() noexcept
{
    // Not in the row the snake starts in, so that no game begins with the head about to go through one, and not beside
    // another portal, so that the two ends of a pair are never next to each other.
    OccupancySet<BOARD_CELLS> blocked = m_Occupied;
    const std::uint8_t row = m_Snake.front().y;
    for (std::uint8_t column = 0; column < BOARD_COLUMNS; ++column)
    {
        blocked.set(to_index(Cell{column, row}));
    }
    PortalCells portals;
    for (Cell& portal : portals.cells)
    {
        const std::size_t empty_cells = BOARD_CELLS - blocked.count();
        const auto rank = static_cast<std::size_t>(m_Random.below(static_cast<std::uint32_t>(empty_cells)));
        portal = from_index(static_cast<std::uint16_t>(blocked.nth_clear(rank)));
        blocked.set(to_index(portal));
        for (const Direction direction : ALL_DIRECTIONS)
        {
            if (const std::optional<Cell> neighbour = step(portal, direction, EdgeRule::WALLS))
            {
                blocked.set(to_index(*neighbour));
            }
        }
    }
    m_Portals = portals;
}

} // namespace snake::sim
