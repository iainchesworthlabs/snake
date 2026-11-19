// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/tally.hpp"

#include <algorithm>
#include <limits>

namespace snake::sim
{

namespace
{

constexpr std::uint16_t COUNT_MAX = std::numeric_limits<std::uint16_t>::max();

void count_up(std::uint16_t& counter) noexcept
{
    if (counter < COUNT_MAX)
    {
        ++counter;
    }
}

/// True when two cells are not next to each other on the board, however the edge is treated.
constexpr bool apart(Cell from, Cell to) noexcept
{
    const int dx = from.x > to.x ? from.x - to.x : to.x - from.x;
    const int dy = from.y > to.y ? from.y - to.y : to.y - from.y;
    return dx + dy != 1;
}

} // namespace

void Tally::start(Cell head) noexcept
{
    *this = Tally{};
    m_Head = head;
}

void Tally::observe(TickOutcome outcome, const Simulation& game) noexcept
{
    switch (outcome)
    {
    case TickOutcome::NOT_RUNNING:
        return;
    case TickOutcome::MOVED:
        break;
    case TickOutcome::ATE:
    case TickOutcome::WON:
        count_up(m_Blueberries);
        m_TopCombo = std::max(m_TopCombo, game.combo());
        if (outcome == TickOutcome::WON)
        {
            m_EndedBy = Ending::FULL_BOARD;
        }
        break;
    case TickOutcome::ATE_BONUS:
        count_up(m_BonusItems);
        break;
    case TickOutcome::ATE_POTATO:
        count_up(m_BombsEaten);
        break;
    case TickOutcome::TELEPORTED:
        count_up(m_PortalsUsed);
        break;
    case TickOutcome::HIT_WALL:
        m_EndedBy = Ending::WALL;
        break;
    case TickOutcome::HIT_SELF:
        m_EndedBy = Ending::SELF;
        break;
    case TickOutcome::EXPLODED:
        m_EndedBy = Ending::BOMB;
        break;
    }
    // The simulation keeps its count in 16 bits, so this cannot lose anything.
    m_FoodsEaten = static_cast<std::uint16_t>(game.foods_eaten());

    // The snake does not move on a tick that ends the game by crashing or by the bomb.
    const bool moved =
        outcome != TickOutcome::HIT_WALL && outcome != TickOutcome::HIT_SELF && outcome != TickOutcome::EXPLODED;
    if (moved)
    {
        if (outcome != TickOutcome::TELEPORTED && game.edge_rule() == EdgeRule::WRAP && apart(m_Head, game.head()))
        {
            m_Wrapped = true;
        }
        m_Head = game.head();
    }
}

void Tally::note_shed() noexcept
{
    count_up(m_Sheds);
}

} // namespace snake::sim
