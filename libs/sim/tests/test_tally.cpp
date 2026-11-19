// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/autoplayer.hpp"
#include "snake/sim/scoring.hpp"
#include "snake/sim/simulation.hpp"
#include "snake/sim/tally.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

using namespace snake::sim;

namespace
{

constexpr std::uint16_t COUNT_MAX = std::numeric_limits<std::uint16_t>::max();

/// A game and the tally that watches it, as the application runs them.
struct Watched
{
    Simulation game;
    Tally tally;

    explicit Watched(const Simulation& start) : game(start)
    {
        tally.start(game.head());
    }

    TickOutcome step()
    {
        const TickOutcome outcome = game.tick();
        tally.observe(outcome, game);
        return outcome;
    }
};

Simulation required(const Simulation::Setup& setup)
{
    const std::optional<Simulation> game = Simulation::from_setup(setup);
    REQUIRE(game.has_value());
    return *game;
}

/// Three parts on row 5, head at (5, 5) heading right, and food straight ahead at (6, 5).
Simulation::Setup row_setup(EdgeRule edges = EdgeRule::WALLS)
{
    static const std::vector<Cell> SNAKE{{5, 5}, {4, 5}, {3, 5}};
    return {.edges = edges, .heading = Direction::RIGHT, .snake = SNAKE, .food = {6, 5}, .seed = 11};
}

/// Three parts on row `y`, head at (`x`, `y`) heading right, with the food on `food`.
Simulation along_row(EdgeRule edges, std::uint8_t x, std::uint8_t y, Cell food)
{
    const std::vector<Cell> snake{{x, y}, {static_cast<std::uint8_t>(x - 1), y}, {static_cast<std::uint8_t>(x - 2), y}};
    return required({.edges = edges, .heading = Direction::RIGHT, .snake = snake, .food = food, .seed = 5});
}

/// The first `count` cells of a snake that fills the board row by row, head first (as the scoring tests build it).
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

/// Six parts on row 8, head first at (8, 8), heading right.
std::vector<Cell> potato_snake()
{
    std::vector<Cell> snake;
    for (std::uint8_t part = 0; part < 6; ++part)
    {
        snake.push_back({static_cast<std::uint8_t>(8 - part), 8});
    }
    return snake;
}

/// A game with the head in the right column, heading right.
Simulation at_right_edge(EdgeRule edges)
{
    return along_row(edges, BOARD_COLUMNS - 1, 5, {10, 10});
}

Tally started_at(Cell head)
{
    Tally tally;
    tally.start(head);
    return tally;
}

} // namespace

TEST_CASE("A tally that has seen nothing is empty", "[tally]")
{
    const Tally tally = started_at({10, 10});
    CHECK(tally.blueberries() == 0);
    CHECK(tally.bonus_items() == 0);
    CHECK(tally.bombs_eaten() == 0);
    CHECK(tally.sheds() == 0);
    CHECK(tally.portals_used() == 0);
    CHECK(tally.top_combo() == 0);
    CHECK(tally.foods_eaten() == 0);
    CHECK_FALSE(tally.wrapped());
    CHECK(tally.ended_by() == Ending::NONE);
}

TEST_CASE("Moving without eating counts nothing", "[tally]")
{
    Watched run(required(row_setup()));
    // The food is one cell ahead, so turn away from it first.
    REQUIRE(run.game.turn(Direction::UP));
    REQUIRE(run.step() == TickOutcome::MOVED);
    CHECK(run.tally.blueberries() == 0);
    CHECK(run.tally.bonus_items() == 0);
    CHECK(run.tally.top_combo() == 0);
    CHECK(run.tally.foods_eaten() == 0);
    CHECK_FALSE(run.tally.wrapped());
    CHECK(run.tally.ended_by() == Ending::NONE);
}

TEST_CASE("Eating a blueberry counts it, the foods eaten and a combo of 1", "[tally]")
{
    Watched run(required(row_setup()));
    REQUIRE(run.step() == TickOutcome::ATE);
    CHECK(run.tally.blueberries() == 1);
    CHECK(run.tally.foods_eaten() == 1);
    CHECK(run.tally.top_combo() == 1);
    CHECK(run.tally.bonus_items() == 0);
    CHECK(run.tally.ended_by() == Ending::NONE);
}

TEST_CASE("The top combo reaches 5 after five foods in the window and stays at 4 after four", "[tally]")
{
    // The autoplayer takes the shortest way to each food, and on a board that wraps no food is more than 20 steps
    // away, so the first five foods of this game follow one another inside the window.
    Watched run(Simulation(EdgeRule::WRAP, 1));
    while (run.tally.blueberries() < 4)
    {
        static_cast<void>(run.game.turn(autoplay(run.game)));
        REQUIRE(run.step() != TickOutcome::NOT_RUNNING);
        REQUIRE(run.game.running());
    }
    CHECK(run.tally.top_combo() == 4);
    CHECK(run.game.combo() == 4);
    while (run.tally.blueberries() < 5)
    {
        static_cast<void>(run.game.turn(autoplay(run.game)));
        REQUIRE(run.step() != TickOutcome::NOT_RUNNING);
        REQUIRE(run.game.running());
    }
    CHECK(run.tally.top_combo() == Scoring::MAX_COMBO);
    CHECK(run.tally.blueberries() == 5);
}

TEST_CASE("The top combo is the largest seen, not the last", "[tally]")
{
    // Eating at a combo of 2 raises it to 3, and eating again after the window has passed starts from 1.
    Simulation::Setup warm = row_setup();
    warm.scoring = {.score = 40, .foods_eaten = 3, .combo = 2, .ticks_since_food = 2};
    Watched run(required(warm));
    REQUIRE(run.step() == TickOutcome::ATE);
    CHECK(run.tally.top_combo() == 3);

    Simulation::Setup cold = row_setup();
    cold.scoring = {.score = 40, .foods_eaten = 4, .combo = 4, .ticks_since_food = Scoring::LAPSED};
    Simulation game = required(cold);
    const TickOutcome outcome = game.tick();
    REQUIRE(outcome == TickOutcome::ATE);
    REQUIRE(game.combo() == 1);
    run.tally.observe(outcome, game);
    CHECK(run.tally.top_combo() == 3);
    CHECK(run.tally.blueberries() == 2);
}

TEST_CASE("A bonus item counts as a bonus item and not as a blueberry", "[tally]")
{
    Simulation::Setup setup = row_setup();
    setup.food = {15, 15};
    setup.bonus = Simulation::Bonus{.cell = {6, 5}, .ticks_left = 9};
    Watched run(required(setup));
    REQUIRE(run.step() == TickOutcome::ATE_BONUS);
    CHECK(run.tally.bonus_items() == 1);
    CHECK(run.tally.blueberries() == 0);
    CHECK(run.tally.top_combo() == 0);
    CHECK(run.tally.foods_eaten() == 1); // the speed curve counts a bonus item as a food
}

TEST_CASE("A bomb counts as a bomb and not as a blueberry, and a shed is counted by the caller", "[tally]")
{
    const std::vector<Cell> snake = potato_snake();
    Watched run(required({.snake = snake,
                          .food = {1, 1},
                          .seed = 1,
                          .variant = Variant::HOT_POTATO,
                          .potato = Simulation::Potato{.cell = {9, 8}, .ticks_left = 12}}));
    REQUIRE(run.step() == TickOutcome::ATE_POTATO);
    CHECK(run.tally.bombs_eaten() == 1);
    CHECK(run.tally.blueberries() == 0);
    CHECK(run.tally.top_combo() == 0);
    CHECK(run.tally.sheds() == 0);

    REQUIRE(run.step() == TickOutcome::MOVED);
    REQUIRE(run.game.shed());
    run.tally.note_shed();
    CHECK(run.tally.sheds() == 1);
    CHECK(run.tally.bombs_eaten() == 1);
    CHECK(run.tally.ended_by() == Ending::NONE);
}

TEST_CASE("A head that crosses the edge of a board that wraps sets wrapped", "[tally][wrap]")
{
    Watched run(at_right_edge(EdgeRule::WRAP));
    CHECK_FALSE(run.tally.wrapped());
    REQUIRE(run.step() == TickOutcome::MOVED);
    CHECK(run.game.head() == Cell{0, 5});
    CHECK(run.tally.wrapped());

    // It stays set.
    REQUIRE(run.step() == TickOutcome::MOVED);
    CHECK(run.tally.wrapped());
}

TEST_CASE("Wrapping while eating, and over the top edge, sets wrapped", "[tally][wrap]")
{
    Watched eating(along_row(EdgeRule::WRAP, BOARD_COLUMNS - 1, 5, {0, 5}));
    REQUIRE(eating.step() == TickOutcome::ATE);
    CHECK(eating.tally.wrapped());

    const std::vector<Cell> snake{{7, 0}, {7, 1}, {7, 2}};
    Watched vertical(
        required({.edges = EdgeRule::WRAP, .heading = Direction::UP, .snake = snake, .food = {10, 10}, .seed = 1}));
    REQUIRE(vertical.step() == TickOutcome::MOVED);
    CHECK(vertical.game.head() == Cell{7, BOARD_ROWS - 1});
    CHECK(vertical.tally.wrapped());
}

TEST_CASE("Ordinary moves on a board that wraps do not set wrapped", "[tally][wrap]")
{
    Watched run(required(row_setup(EdgeRule::WRAP)));
    REQUIRE(run.step() == TickOutcome::ATE);
    REQUIRE(run.step() == TickOutcome::MOVED);
    CHECK_FALSE(run.tally.wrapped());
}

TEST_CASE("The same move against a wall ends the game and does not set wrapped", "[tally][wrap]")
{
    Watched run(at_right_edge(EdgeRule::WALLS));
    REQUIRE(run.step() == TickOutcome::HIT_WALL);
    CHECK_FALSE(run.tally.wrapped());
    CHECK(run.tally.ended_by() == Ending::WALL);
}

TEST_CASE("A tick that ends the game does not count as a move for the wrap check", "[tally][wrap]")
{
    // The head is at (19, 5) on a board that wraps, with its own body on the cell it would wrap to.
    const std::vector<Cell> snake{{19, 5}, {19, 6}, {0, 6}, {0, 5}, {1, 5}};
    Watched run(
        required({.edges = EdgeRule::WRAP, .heading = Direction::RIGHT, .snake = snake, .food = {10, 10}, .seed = 1}));
    REQUIRE(run.step() == TickOutcome::HIT_SELF);
    CHECK_FALSE(run.tally.wrapped());
    CHECK(run.tally.ended_by() == Ending::SELF);
}

TEST_CASE("A portal counts as a portal and does not set wrapped", "[tally][wrap]")
{
    const Simulation::PortalCells portals{{Cell{5, 5}, Cell{15, 3}, Cell{2, 12}, Cell{17, 15}}};
    const std::vector<Cell> snake{{4, 5}, {3, 5}, {2, 5}};
    const auto portal_game = [&](EdgeRule edges)
    {
        return required({.edges = edges,
                         .heading = Direction::RIGHT,
                         .snake = snake,
                         .food = {10, 10},
                         .seed = 1,
                         .variant = Variant::PORTALS,
                         .portals = portals});
    };

    Watched walls(portal_game(EdgeRule::WALLS));
    REQUIRE(walls.step() == TickOutcome::TELEPORTED);
    CHECK(walls.tally.portals_used() == 1);
    CHECK_FALSE(walls.tally.wrapped());
    CHECK(walls.tally.blueberries() == 0);

    // Not even where the edges wrap and the head lands far from the cell it left.
    Watched wrapping(portal_game(EdgeRule::WRAP));
    REQUIRE(wrapping.step() == TickOutcome::TELEPORTED);
    CHECK(wrapping.tally.portals_used() == 1);
    CHECK_FALSE(wrapping.tally.wrapped());
    // The next step is measured from the portal's exit.
    REQUIRE(wrapping.step() == TickOutcome::MOVED);
    CHECK_FALSE(wrapping.tally.wrapped());
}

TEST_CASE("Each way a game can end is recorded", "[tally][ending]")
{
    {
        Watched run(at_right_edge(EdgeRule::WALLS));
        REQUIRE(run.step() == TickOutcome::HIT_WALL);
        CHECK(run.tally.ended_by() == Ending::WALL);
    }
    {
        const std::vector<Cell> snake{{5, 5}, {6, 5}, {6, 6}, {5, 6}, {4, 6}};
        Watched run(required({.heading = Direction::DOWN, .snake = snake, .food = {0, 0}}));
        REQUIRE(run.step() == TickOutcome::HIT_SELF);
        CHECK(run.tally.ended_by() == Ending::SELF);
    }
    {
        const std::vector<Cell> snake = potato_snake();
        Watched run(required(
            {.snake = snake, .food = {1, 1}, .seed = 1, .variant = Variant::HOT_POTATO, .lump = snake.front()}));
        for (int tick = 1; tick <= 5; ++tick)
        {
            REQUIRE(run.step() == TickOutcome::MOVED);
            CHECK(run.tally.ended_by() == Ending::NONE);
        }
        REQUIRE(run.step() == TickOutcome::EXPLODED);
        CHECK(run.tally.ended_by() == Ending::BOMB);
    }
    {
        const std::vector<Cell> snake = serpentine(399);
        Simulation::Setup setup{.heading = Direction::LEFT, .snake = snake, .food = {0, 19}, .seed = 3};
        setup.scoring = {.score = 3000, .foods_eaten = 396, .combo = 2, .ticks_since_food = 1};
        Watched run(required(setup));
        REQUIRE(run.step() == TickOutcome::WON);
        CHECK(run.tally.ended_by() == Ending::FULL_BOARD);
        CHECK(run.tally.blueberries() == 1); // the last food counts
        CHECK(run.tally.top_combo() == 3);
        CHECK(run.tally.foods_eaten() == 397);
    }
}

TEST_CASE("A tick after the game has ended changes nothing", "[tally][ending]")
{
    Watched run(at_right_edge(EdgeRule::WALLS));
    REQUIRE(run.step() == TickOutcome::HIT_WALL);
    const Tally before = run.tally;
    REQUIRE(run.step() == TickOutcome::NOT_RUNNING);
    CHECK(run.tally == before);
    CHECK(run.tally.ended_by() == Ending::WALL);
}

TEST_CASE("Starting again clears everything", "[tally]")
{
    Watched run(at_right_edge(EdgeRule::WRAP));
    REQUIRE(run.step() == TickOutcome::MOVED);
    run.tally.note_shed();
    REQUIRE(run.tally.wrapped());

    Simulation::Setup setup = row_setup();
    setup.scoring = {.score = 40, .foods_eaten = 3, .combo = 2, .ticks_since_food = 2};
    Simulation eating = required(setup);
    const TickOutcome outcome = eating.tick();
    run.tally.observe(outcome, eating);
    REQUIRE(run.tally.blueberries() == 1);
    REQUIRE(run.tally.top_combo() == 3);

    run.tally.start({5, 5});
    CHECK(run.tally == started_at({5, 5}));
    CHECK(run.tally.sheds() == 0);
    CHECK_FALSE(run.tally.wrapped());
    CHECK(run.tally.top_combo() == 0);
    CHECK(run.tally.foods_eaten() == 0);
    CHECK(run.tally.blueberries() == 0);
}

TEST_CASE("Tallies that differ compare unequal", "[tally]")
{
    Tally one = started_at({1, 1});
    Tally two = one;
    CHECK(one == two);
    two.note_shed();
    CHECK(one != two);
}

TEST_CASE("The counters stop at their largest value instead of wrapping round", "[tally]")
{
    Simulation ate = required(row_setup());
    REQUIRE(ate.tick() == TickOutcome::ATE);

    Tally tally = started_at({5, 5});
    const std::uint32_t calls = static_cast<std::uint32_t>(COUNT_MAX) + 100;
    for (std::uint32_t call = 0; call < calls; ++call)
    {
        tally.observe(TickOutcome::ATE, ate);
        tally.observe(TickOutcome::ATE_BONUS, ate);
        tally.observe(TickOutcome::ATE_POTATO, ate);
        tally.observe(TickOutcome::TELEPORTED, ate);
        tally.note_shed();
    }
    CHECK(tally.blueberries() == COUNT_MAX);
    CHECK(tally.bonus_items() == COUNT_MAX);
    CHECK(tally.bombs_eaten() == COUNT_MAX);
    CHECK(tally.portals_used() == COUNT_MAX);
    CHECK(tally.sheds() == COUNT_MAX);
}
