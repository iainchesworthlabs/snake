// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/random.hpp"
#include "snake/sim/simulation.hpp"
#include "snake/sim/snapshot.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

using namespace snake::sim;

namespace
{

constexpr std::array<Direction, 4> DIRECTIONS = {Direction::UP, Direction::DOWN, Direction::LEFT, Direction::RIGHT};

bool on_snake(const Simulation& game, Cell cell)
{
    for (std::size_t part = 0; part < game.length(); ++part)
    {
        if (game.snake()[part] == cell)
        {
            return true;
        }
    }
    return false;
}

bool one_step_apart(Cell from, Cell to)
{
    const int dx = from.x - to.x;
    const int dy = from.y - to.y;
    return (dx * dx) + (dy * dy) == 1;
}

/// Every rule of a position that must hold after any tick or shed, in any variant. Returns what is wrong, or an empty
/// string.
const char* what_is_wrong(const Simulation& game)
{
    if (game.length() == 0)
    {
        return "the snake is empty";
    }
    for (std::size_t first = 0; first < game.length(); ++first)
    {
        if (!on_board(game.snake()[first]))
        {
            return "a part is off the board";
        }
        for (std::size_t second = first + 1; second < game.length(); ++second)
        {
            if (game.snake()[first] == game.snake()[second])
            {
                return "two parts are on one cell";
            }
        }
    }
    // Each part is one step from the part before it, or the head came out of a portal that the part before it went
    // into.
    for (std::size_t part = 1; part < game.length(); ++part)
    {
        const Cell newer = game.snake()[part - 1];
        const Cell older = game.snake()[part];
        bool linked = one_step_apart(older, newer);
        if (!linked && game.portals())
        {
            for (const Direction direction : DIRECTIONS)
            {
                int x = older.x;
                int y = older.y;
                x += direction == Direction::LEFT ? -1 : direction == Direction::RIGHT ? 1 : 0;
                y += direction == Direction::UP ? -1 : direction == Direction::DOWN ? 1 : 0;
                if (x >= 0 && y >= 0 && x < BOARD_COLUMNS && y < BOARD_ROWS &&
                    game.portals()->exit_from(Cell{static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y)}) ==
                        newer)
                {
                    linked = true;
                }
            }
        }
        if (!linked)
        {
            return "a part is not next to the one before it";
        }
    }
    if (!game.won() && on_snake(game, game.food()))
    {
        return "the food is on the snake";
    }
    if (game.bonus() && (on_snake(game, game.bonus()->cell) || game.bonus()->cell == game.food()))
    {
        return "the bonus item is on the snake or the food";
    }
    if (game.potato())
    {
        if (on_snake(game, game.potato()->cell) || game.potato()->cell == game.food() ||
            (game.bonus() && game.potato()->cell == game.bonus()->cell))
        {
            return "the bomb food is on the snake or on another item";
        }
        if (game.lump())
        {
            return "a bomb on the board and a bomb in the snake";
        }
    }
    if (game.lump() && !on_snake(game, *game.lump()))
    {
        return "the bomb is not in the body";
    }
    if (game.portals())
    {
        for (const Cell portal : game.portals()->cells)
        {
            if (portal == game.food() || (game.bonus() && portal == game.bonus()->cell) ||
                (game.potato() && portal == game.potato()->cell))
            {
                return "an item is on a portal";
            }
        }
    }
    if (game.variant() != Variant::HOT_POTATO && (game.potato() || game.lump()))
    {
        return "a bomb in a game without bombs";
    }
    if (game.variant() != Variant::PORTALS && game.portals())
    {
        return "portals in a game without portals";
    }
    return nullptr;
}

} // namespace

TEST_CASE("Random play in every variant keeps the rules of the position, and a saved game plays on the same",
          "[simulation][variants][properties]")
{
    for (const Variant variant : {Variant::PLAIN, Variant::PORTALS, Variant::HOT_POTATO})
    {
        for (std::uint64_t seed = 1; seed <= 25; ++seed)
        {
            Simulation game(EdgeRule::WALLS, seed, variant);
            Random dice(seed * 7919);
            for (int tick = 0; tick < 1500 && game.running(); ++tick)
            {
                const std::uint32_t roll = dice.below(100);
                if (roll < 30)
                {
                    static_cast<void>(game.turn(DIRECTIONS[dice.below(4)]));
                }
                if (roll >= 90)
                {
                    static_cast<void>(game.shed());
                }
                static_cast<void>(game.tick());
                if (const char* wrong = what_is_wrong(game))
                {
                    INFO("variant " << static_cast<int>(variant) << " seed " << seed << " tick " << tick);
                    FAIL(wrong);
                }
                if (tick % 50 == 0)
                {
                    const std::optional<Simulation> copy = restore(capture(game));
                    REQUIRE(copy.has_value());
                    REQUIRE(state_hash(*copy) == state_hash(game));
                    REQUIRE(restore(*decode(
                                        [&]
                                        {
                                            std::array<std::uint8_t, MAX_ENCODED_SNAPSHOT_SIZE> bytes{};
                                            const std::size_t size = encode(capture(game), bytes);
                                            return std::vector<std::uint8_t>(
                                                bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(size));
                                        }()))
                                .has_value());
                }
            }
        }
    }
}
