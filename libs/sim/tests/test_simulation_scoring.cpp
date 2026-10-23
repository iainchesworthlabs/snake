// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/simulation.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

using namespace snake::sim;

namespace
{

/// A game in an exact position, which must be a legal one.
Simulation game_from(Simulation::Setup setup, std::vector<Cell> snake)
{
    setup.snake = snake;
    const std::optional<Simulation> game = Simulation::from_setup(setup);
    REQUIRE(game.has_value());
    return *game;
}

/// A short snake on row 5 with its head at (5, 5), heading right, with food in front of it.
Simulation::Setup short_snake(Scoring::State scoring = {}, std::optional<Simulation::Bonus> bonus = std::nullopt)
{
    return {.edges = EdgeRule::WALLS,
            .heading = Direction::RIGHT,
            .snake = {},
            .food = {6, 5},
            .seed = 11,
            .scoring = scoring,
            .bonus = bonus};
}

const std::vector<Cell> SHORT_SNAKE{{5, 5}, {4, 5}, {3, 5}};

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

/// The first `count` cells of a snake that fills the board row by row, head first. Row 19 runs from right to left,
/// so a snake whose head is on it heads left towards the last empty cells.
std::vector<Cell> serpentine(std::size_t count)
{
    std::vector<Cell> path;
    for (std::uint8_t row = 0; row < BOARD_ROWS; ++row)
    {
        for (std::uint8_t column = 0; column < BOARD_COLUMNS; ++column)
        {
            path.push_back(Cell{static_cast<std::uint8_t>(row % 2 == 0 ? column : BOARD_COLUMNS - 1 - column), row});
        }
    }
    path.resize(count);
    return std::vector<Cell>(path.rbegin(), path.rend());
}

Simulation::Setup full_board_setup(Cell food, Scoring::State scoring = {},
                                   std::optional<Simulation::Bonus> bonus = std::nullopt)
{
    return {.edges = EdgeRule::WALLS,
            .heading = Direction::LEFT,
            .snake = {},
            .food = food,
            .seed = 3,
            .scoring = scoring,
            .bonus = bonus};
}

} // namespace

TEST_CASE("A new game has no score and a combo of x1", "[simulation][score]")
{
    const Simulation game(EdgeRule::WALLS, 42);
    CHECK(game.score() == 0);
    CHECK(game.combo() == 1);
    CHECK(game.foods_eaten() == 0);
    CHECK_FALSE(game.bonus().has_value());
}

TEST_CASE("Eating food scores 10 points at x1", "[simulation][score]")
{
    Simulation game = game_from(short_snake(), SHORT_SNAKE);
    REQUIRE(game.tick() == TickOutcome::ATE);
    CHECK(game.score() == 10);
    CHECK(game.combo() == 1);
    CHECK(game.foods_eaten() == 1);
}

TEST_CASE("Moving without eating scores nothing", "[simulation][score]")
{
    Simulation game = game_from(short_snake(), SHORT_SNAKE);
    game.turn(Direction::DOWN);
    REQUIRE(game.tick() == TickOutcome::MOVED);
    CHECK(game.score() == 0);
    CHECK(game.foods_eaten() == 0);
}

TEST_CASE("Food eaten within the window raises the multiplier", "[simulation][score][combo]")
{
    // The last food was 3 ticks ago, so this tick is the 4th after it.
    Simulation game =
        game_from(short_snake({.score = 10, .foods_eaten = 1, .combo = 1, .ticks_since_food = 3}), SHORT_SNAKE);
    REQUIRE(game.tick() == TickOutcome::ATE);
    CHECK(game.combo() == 2);
    CHECK(game.score() == 30);
}

TEST_CASE("Food eaten on the 20th tick after the last food still counts", "[simulation][score][combo]")
{
    Simulation game =
        game_from(short_snake({.score = 10, .foods_eaten = 1, .combo = 1, .ticks_since_food = 19}), SHORT_SNAKE);
    REQUIRE(game.tick() == TickOutcome::ATE);
    CHECK(game.combo() == 2);
    CHECK(game.score() == 30);
}

TEST_CASE("Food eaten on the 21st tick after the last food starts again at x1", "[simulation][score][combo]")
{
    Simulation game =
        game_from(short_snake({.score = 60, .foods_eaten = 3, .combo = 3, .ticks_since_food = 20}), SHORT_SNAKE);
    REQUIRE(game.tick() == TickOutcome::ATE);
    CHECK(game.combo() == 1);
    CHECK(game.score() == 70);
}

TEST_CASE("The multiplier stops at x5", "[simulation][score][combo]")
{
    Simulation game =
        game_from(short_snake({.score = 100, .foods_eaten = 5, .combo = 5, .ticks_since_food = 0}), SHORT_SNAKE);
    REQUIRE(game.tick() == TickOutcome::ATE);
    CHECK(game.combo() == 5);
    CHECK(game.score() == 150);
}

TEST_CASE("The multiplier shown drops to x1 when the window passes without food", "[simulation][score][combo]")
{
    Simulation game = game_from({.edges = EdgeRule::WALLS,
                                 .heading = Direction::RIGHT,
                                 .snake = {},
                                 .food = {15, 15},
                                 .seed = 1,
                                 .scoring = {.score = 100, .foods_eaten = 5, .combo = 4, .ticks_since_food = 19}},
                                SHORT_SNAKE);
    REQUIRE(game.tick() == TickOutcome::MOVED);
    CHECK(game.combo() == 4); // 20 ticks since the food: still inside the window
    REQUIRE(game.tick() == TickOutcome::MOVED);
    CHECK(game.combo() == 1);
}

TEST_CASE("The combo clock runs on every tick", "[simulation][score][combo]")
{
    Simulation game = game_from({.edges = EdgeRule::WALLS,
                                 .heading = Direction::RIGHT,
                                 .snake = {},
                                 .food = {15, 15},
                                 .seed = 1,
                                 .scoring = {.combo = 2, .ticks_since_food = 0}},
                                SHORT_SNAKE);
    REQUIRE(game.tick() == TickOutcome::MOVED);
    CHECK(game.scoring().state().ticks_since_food == 1);
    REQUIRE(game.tick() == TickOutcome::MOVED);
    CHECK(game.scoring().state().ticks_since_food == 2);
}

TEST_CASE("The fourth food brings no bonus item and the fifth does", "[simulation][bonus]")
{
    Simulation fourth = game_from(short_snake({.normal_since_bonus = 3}), SHORT_SNAKE);
    REQUIRE(fourth.tick() == TickOutcome::ATE);
    CHECK_FALSE(fourth.bonus().has_value());

    Simulation fifth = game_from(short_snake({.normal_since_bonus = 4}), SHORT_SNAKE);
    REQUIRE(fifth.tick() == TickOutcome::ATE);
    REQUIRE(fifth.bonus().has_value());
    CHECK(fifth.bonus()->ticks_left == Simulation::BONUS_LIFETIME_TICKS);
    CHECK(fifth.scoring().state().normal_since_bonus == 0);
}

TEST_CASE("The bonus item goes on an empty cell of its own, chosen from the seed", "[simulation][bonus]")
{
    // Cells computed apart from the library: list the empty cells in order, take the food by the first draw of the
    // seed and the bonus item by the second.
    struct Pinned
    {
        std::uint64_t seed;
        Cell food;
        Cell bonus;
    };
    const Pinned pinned[] = {{11, {17, 19}, {12, 2}}, {12, {11, 18}, {1, 8}}, {13, {11, 2}, {4, 7}}};
    for (const Pinned& expected : pinned)
    {
        Simulation::Setup setup = short_snake({.normal_since_bonus = 4});
        setup.seed = expected.seed;
        Simulation game = game_from(setup, SHORT_SNAKE);
        REQUIRE(game.tick() == TickOutcome::ATE);
        CHECK(game.food() == expected.food);
        REQUIRE(game.bonus().has_value());
        CHECK(game.bonus()->cell == expected.bonus);
        CHECK_FALSE(on_snake(game, game.bonus()->cell));
    }
}

TEST_CASE("A new bonus item replaces one that is still on the board", "[simulation][bonus]")
{
    Simulation game = game_from(
        short_snake({.normal_since_bonus = 4}, Simulation::Bonus{.cell = {10, 10}, .ticks_left = 5}), SHORT_SNAKE);
    REQUIRE(game.tick() == TickOutcome::ATE);
    REQUIRE(game.bonus().has_value());
    CHECK(game.bonus()->ticks_left == Simulation::BONUS_LIFETIME_TICKS);
    CHECK(game.bonus()->cell != game.food());
    CHECK_FALSE(on_snake(game, game.bonus()->cell));
}

TEST_CASE("Eating the bonus item scores 50 points whatever the combo and grows the snake", "[simulation][bonus]")
{
    // The food sits elsewhere, so the bonus item is what the snake reaches.
    Simulation::Setup setup =
        short_snake({.score = 60, .foods_eaten = 3, .normal_since_bonus = 3, .combo = 3, .ticks_since_food = 2},
                    Simulation::Bonus{.cell = {6, 5}, .ticks_left = 9});
    setup.food = {15, 15};
    Simulation game = game_from(setup, SHORT_SNAKE);
    REQUIRE(game.tick() == TickOutcome::ATE_BONUS);
    CHECK(game.score() == 110);
    CHECK(game.length() == 4);
    CHECK(game.foods_eaten() == 4);
    CHECK_FALSE(game.bonus().has_value());
    CHECK(game.food() == Cell{15, 15});
    CHECK(game.combo() == 3);
    CHECK(game.scoring().state().ticks_since_food == 0);
    CHECK(game.scoring().state().normal_since_bonus == 3);
}

TEST_CASE("The bonus item comes off the board after 40 ticks", "[simulation][bonus]")
{
    // Wrap, so the snake can circle row 5 for as long as the test needs.
    Simulation game = game_from({.edges = EdgeRule::WRAP,
                                 .heading = Direction::RIGHT,
                                 .snake = {},
                                 .food = {0, 10},
                                 .seed = 1,
                                 .bonus = Simulation::Bonus{.cell = {0, 12}}},
                                SHORT_SNAKE);
    for (int tick = 1; tick < 40; ++tick)
    {
        REQUIRE(game.tick() == TickOutcome::MOVED);
        REQUIRE(game.bonus().has_value());
        REQUIRE(game.bonus()->ticks_left == Simulation::BONUS_LIFETIME_TICKS - tick);
    }
    CHECK(game.bonus()->ticks_left == 1);
    REQUIRE(game.tick() == TickOutcome::MOVED);
    CHECK_FALSE(game.bonus().has_value());
}

TEST_CASE("A snake that reaches the bonus item on its last tick eats it", "[simulation][bonus]")
{
    Simulation::Setup setup = short_snake({}, Simulation::Bonus{.cell = {6, 5}, .ticks_left = 1});
    setup.food = {15, 15};
    Simulation game = game_from(setup, SHORT_SNAKE);
    REQUIRE(game.tick() == TickOutcome::ATE_BONUS);
    CHECK(game.score() == 50);
}

TEST_CASE("The bonus item counts down while the snake eats normal food", "[simulation][bonus]")
{
    Simulation game = game_from(short_snake({}, Simulation::Bonus{.cell = {10, 10}, .ticks_left = 7}), SHORT_SNAKE);
    REQUIRE(game.tick() == TickOutcome::ATE);
    REQUIRE(game.bonus().has_value());
    CHECK(game.bonus()->ticks_left == 6);
}

TEST_CASE("A bonus item on its last tick is gone before new food is placed", "[simulation][bonus]")
{
    // With the bonus item gone, its cell is free again for the new food.
    Simulation game =
        game_from(full_board_setup({2, 19}, {}, Simulation::Bonus{.cell = {0, 19}, .ticks_left = 1}), serpentine(397));
    REQUIRE(game.tick() == TickOutcome::ATE);
    CHECK_FALSE(game.bonus().has_value());
    CHECK((game.food() == Cell{1, 19} || game.food() == Cell{0, 19}));
}

TEST_CASE("New food never lands on the bonus item", "[simulation][bonus]")
{
    // Two cells are left after the food is eaten: (1, 19) and the bonus item's (0, 19).
    for (std::uint64_t seed = 0; seed < 30; ++seed)
    {
        Simulation::Setup setup = full_board_setup({2, 19}, {}, Simulation::Bonus{.cell = {0, 19}});
        setup.seed = seed;
        Simulation game = game_from(setup, serpentine(397));
        REQUIRE(game.tick() == TickOutcome::ATE);
        REQUIRE(game.food() == Cell{1, 19});
        REQUIRE(game.bonus().has_value());
        REQUIRE(game.bonus()->cell == Cell{0, 19});
    }
}

TEST_CASE("A bonus item on the last empty cell gives way to the food", "[simulation][bonus]")
{
    Simulation game = game_from(full_board_setup({1, 19}, {}, Simulation::Bonus{.cell = {0, 19}}), serpentine(398));
    REQUIRE(game.tick() == TickOutcome::ATE);
    CHECK(game.running());
    CHECK(game.food() == Cell{0, 19});
    CHECK_FALSE(game.bonus().has_value());
}

TEST_CASE("The bonus item is skipped when no cell is left besides the food", "[simulation][bonus]")
{
    Simulation game = game_from(full_board_setup({1, 19}, {.normal_since_bonus = 4}), serpentine(398));
    REQUIRE(game.tick() == TickOutcome::ATE);
    CHECK(game.food() == Cell{0, 19});
    CHECK_FALSE(game.bonus().has_value());
    CHECK(game.scoring().state().normal_since_bonus == 0);
}

TEST_CASE("The bonus item goes on the one cell that is left besides the food", "[simulation][bonus]")
{
    Simulation game = game_from(full_board_setup({2, 19}, {.normal_since_bonus = 4}), serpentine(397));
    REQUIRE(game.tick() == TickOutcome::ATE);
    REQUIRE(game.bonus().has_value());
    const Cell food = game.food();
    const Cell bonus = game.bonus()->cell;
    CHECK(food != bonus);
    CHECK(((food == Cell{1, 19} && bonus == Cell{0, 19}) || (food == Cell{0, 19} && bonus == Cell{1, 19})));
}

TEST_CASE("The last food of a perfect game still scores", "[simulation][score][win]")
{
    Simulation game =
        game_from(full_board_setup({0, 19}, {.score = 3000, .foods_eaten = 396, .combo = 2, .ticks_since_food = 1}),
                  serpentine(399));
    REQUIRE(game.tick() == TickOutcome::WON);
    CHECK(game.score() == 3030);
    CHECK(game.foods_eaten() == 397);
    CHECK_FALSE(game.bonus().has_value());
}

TEST_CASE("A position with a bonus item that cannot be there is refused", "[simulation][setup][bonus]")
{
    const auto refused = [](std::optional<Simulation::Bonus> bonus)
    {
        Simulation::Setup setup = short_snake({}, bonus);
        setup.snake = SHORT_SNAKE;
        return !Simulation::from_setup(setup).has_value();
    };
    CHECK_FALSE(refused(Simulation::Bonus{.cell = {10, 10}}));
    CHECK_FALSE(refused(Simulation::Bonus{.cell = {10, 10}, .ticks_left = 1}));
    CHECK_FALSE(refused(Simulation::Bonus{.cell = {10, 10}, .ticks_left = Simulation::BONUS_LIFETIME_TICKS}));
    CHECK(refused(Simulation::Bonus{.cell = {20, 10}}));
    CHECK(refused(Simulation::Bonus{.cell = {4, 5}}));
    CHECK(refused(Simulation::Bonus{.cell = {6, 5}})); // the food's cell
    CHECK(refused(Simulation::Bonus{.cell = {10, 10}, .ticks_left = 0}));
    CHECK(refused(Simulation::Bonus{.cell = {10, 10}, .ticks_left = Simulation::BONUS_LIFETIME_TICKS + 1}));
}

TEST_CASE("A position with scoring that cannot happen is refused", "[simulation][setup][score]")
{
    Simulation::Setup setup = short_snake({.combo = 0});
    setup.snake = SHORT_SNAKE;
    CHECK_FALSE(Simulation::from_setup(setup).has_value());
}
