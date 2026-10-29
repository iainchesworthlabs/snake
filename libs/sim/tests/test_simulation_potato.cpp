// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/autoplayer.hpp"
#include "snake/sim/simulation.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <optional>
#include <vector>

using namespace snake::sim;

namespace
{

/// A snake of `length` parts along row 8, head first at (`length` + 2, 8) heading right. The food is far from it.
std::vector<Cell> row_snake(std::uint8_t length)
{
    std::vector<Cell> snake;
    for (std::uint8_t part = 0; part < length; ++part)
    {
        snake.push_back({static_cast<std::uint8_t>(length + 2 - part), 8});
    }
    return snake;
}

Simulation::Setup potato_setup(const std::vector<Cell>& snake, Cell food)
{
    return {.snake = snake, .food = food, .seed = 1, .variant = Variant::HOT_POTATO};
}

Simulation required(const Simulation::Setup& setup)
{
    const std::optional<Simulation> game = Simulation::from_setup(setup);
    REQUIRE(game.has_value());
    return *game;
}

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

/// A game of 6 parts with a bomb swallowed in the head, heading right along row 8 with room ahead.
Simulation with_bomb_in_head()
{
    const std::vector<Cell> snake = row_snake(6);
    Simulation::Setup setup = potato_setup(snake, {1, 1});
    setup.lump = snake.front();
    return required(setup);
}

} // namespace

TEST_CASE("The third normal food of a long enough snake is followed by a bomb food", "[potato]")
{
    const std::vector<Cell> snake = row_snake(6);
    Simulation::Setup setup = potato_setup(snake, {9, 8});
    setup.normal_since_potato = 2;
    Simulation game = required(setup);

    REQUIRE(game.tick() == TickOutcome::ATE);
    REQUIRE(game.potato().has_value());
    const Simulation::Potato potato = *game.potato();
    CHECK(potato.ticks_left == Simulation::POTATO_LIFETIME_TICKS);
    CHECK(on_board(potato.cell));
    CHECK_FALSE(on_snake(game, potato.cell));
    CHECK(potato.cell != game.food());
    CHECK(game.normal_since_potato() == 0);
    CHECK_FALSE(game.lump().has_value());
}

TEST_CASE("The bomb food waits for a snake of 6 parts, and comes with the next food", "[potato]")
{
    const std::vector<Cell> short_snake = row_snake(4);
    Simulation::Setup setup = potato_setup(short_snake, {7, 8});
    setup.normal_since_potato = 2;
    Simulation game = required(setup);

    REQUIRE(game.tick() == TickOutcome::ATE);
    CHECK(game.length() == 5);
    CHECK_FALSE(game.potato().has_value()); // the snake is 5 parts after it ate the third
    CHECK(game.normal_since_potato() == Simulation::POTATO_EVERY);

    // The next food is the first with a long enough snake.
    const std::vector<Cell> long_snake = row_snake(5);
    Simulation::Setup next = potato_setup(long_snake, {8, 8});
    next.normal_since_potato = Simulation::POTATO_EVERY;
    Simulation second = required(next);
    REQUIRE(second.tick() == TickOutcome::ATE);
    CHECK(second.potato().has_value());
    CHECK(second.normal_since_potato() == 0);
}

TEST_CASE("Only the variant with bomb food has it", "[potato]")
{
    const std::vector<Cell> snake = row_snake(6);
    Simulation plain = required({.snake = snake, .food = {9, 8}, .seed = 1});
    REQUIRE(plain.tick() == TickOutcome::ATE);
    CHECK_FALSE(plain.potato().has_value());
    CHECK(plain.normal_since_potato() == 0);
}

TEST_CASE("No second bomb food comes while one is on the board or in the snake", "[potato]")
{
    const std::vector<Cell> snake = row_snake(6);
    {
        Simulation::Setup setup = potato_setup(snake, {9, 8});
        setup.normal_since_potato = 3;
        setup.potato = Simulation::Potato{.cell = {1, 1}};
        Simulation game = required(setup);
        REQUIRE(game.tick() == TickOutcome::ATE);
        CHECK(game.potato() == Simulation::Potato{.cell = {1, 1}, .ticks_left = 49});
    }
    {
        Simulation::Setup setup = potato_setup(snake, {9, 8});
        setup.normal_since_potato = 3;
        setup.lump = snake[3];
        Simulation game = required(setup);
        REQUIRE(game.tick() == TickOutcome::ATE);
        CHECK_FALSE(game.potato().has_value());
        CHECK(game.lump() == snake[3]);
    }
}

TEST_CASE("Eating the bomb food scores 100, grows the snake and puts the bomb in the head", "[potato]")
{
    const std::vector<Cell> snake = row_snake(6);
    Simulation::Setup setup = potato_setup(snake, {1, 1});
    setup.potato = Simulation::Potato{.cell = {9, 8}, .ticks_left = 12};
    Simulation game = required(setup);

    REQUIRE(game.tick() == TickOutcome::ATE_POTATO);
    CHECK(game.score() == Scoring::POTATO_POINTS);
    CHECK(game.length() == 7);
    CHECK(game.foods_eaten() == 1);
    CHECK_FALSE(game.potato().has_value());
    CHECK(game.lump() == Cell{9, 8});
    CHECK(game.lump_index() == 0);
    CHECK(game.running());
}

TEST_CASE("The bomb stays on its cell and the body moves past it", "[potato]")
{
    Simulation game = with_bomb_in_head();
    const Cell bomb = game.head();
    for (std::size_t tick = 1; tick <= 4; ++tick)
    {
        REQUIRE(game.tick() == TickOutcome::MOVED);
        CHECK(game.lump() == bomb);
        CHECK(game.lump_index() == tick);
    }
}

TEST_CASE("A bomb that reaches the tail as the tail moves away goes off", "[potato]")
{
    Simulation game = with_bomb_in_head();
    // The snake has 6 parts, so the bomb is on the tail after 5 moves, and the sixth tick would move the tail away.
    for (int tick = 1; tick <= 5; ++tick)
    {
        REQUIRE(game.tick() == TickOutcome::MOVED);
    }
    REQUIRE(game.lump_index() == 5);
    const Cell head = game.head();
    CHECK(game.tick() == TickOutcome::EXPLODED);
    CHECK_FALSE(game.running());
    CHECK_FALSE(game.won());
    CHECK(game.head() == head); // the snake is where it was
    CHECK(game.tick() == TickOutcome::NOT_RUNNING);
}

TEST_CASE("Eating keeps the tail where it is, so the bomb waits one more tick", "[potato]")
{
    const std::vector<Cell> snake = row_snake(6);
    Simulation::Setup setup = potato_setup(snake, {1, 1});
    setup.lump = snake.back(); // on the tail already
    // Food right ahead of the head.
    setup.food = {9, 8};
    Simulation game = required(setup);
    REQUIRE(game.lump_index() == 5);
    REQUIRE(game.tick() == TickOutcome::ATE);
    CHECK(game.running());
    CHECK(game.lump() == snake.back());
    CHECK(game.tick() == TickOutcome::EXPLODED);
}

TEST_CASE("Shedding drops the bomb and everything behind it, and frees the cells", "[potato][shed]")
{
    Simulation game = with_bomb_in_head();
    REQUIRE(game.tick() == TickOutcome::MOVED);
    REQUIRE(game.tick() == TickOutcome::MOVED);
    REQUIRE(game.lump_index() == 2);
    const std::vector<Cell> dropped{game.snake()[2], game.snake()[3], game.snake()[4], game.snake()[5]};

    CHECK(game.shed());
    CHECK(game.length() == 2);
    CHECK_FALSE(game.lump().has_value());
    for (const Cell cell : dropped)
    {
        CHECK_FALSE(on_snake(game, cell));
    }
    // The game goes on, and the cells can be used again: the snake can move through where its tail was.
    CHECK(game.tick() == TickOutcome::MOVED);
    CHECK(game.running());
}

TEST_CASE("Shedding as late as it can be keeps all but the tail", "[potato][shed]")
{
    Simulation game = with_bomb_in_head();
    for (int tick = 1; tick <= 5; ++tick)
    {
        REQUIRE(game.tick() == TickOutcome::MOVED);
    }
    CHECK(game.shed());
    CHECK(game.length() == 5);
    CHECK(game.tick() == TickOutcome::MOVED);
}

TEST_CASE("Shedding does nothing when there is no bomb, when it is in the head and when the game is over",
          "[potato][shed]")
{
    const std::vector<Cell> snake = row_snake(6);
    Simulation plain = required({.snake = snake, .food = {1, 1}, .seed = 1});
    CHECK_FALSE(plain.shed());
    CHECK(plain.length() == 6);

    Simulation no_bomb = required(potato_setup(snake, {1, 1}));
    CHECK_FALSE(no_bomb.shed());

    Simulation game = with_bomb_in_head();
    CHECK_FALSE(game.shed()); // in the head: that would drop the whole snake
    CHECK(game.length() == 6);
    CHECK(game.lump().has_value());

    for (int tick = 1; tick <= 6; ++tick)
    {
        static_cast<void>(game.tick());
    }
    REQUIRE_FALSE(game.running());
    CHECK_FALSE(game.shed());
}

TEST_CASE("The bomb food goes away on its own when its time is up, unless it is eaten on the last tick", "[potato]")
{
    const std::vector<Cell> snake = row_snake(6);
    {
        Simulation::Setup setup = potato_setup(snake, {1, 1});
        setup.edges = EdgeRule::WRAP;
        setup.potato = Simulation::Potato{.cell = {3, 3}, .ticks_left = 2};
        Simulation game = required(setup);
        REQUIRE(game.tick() == TickOutcome::MOVED);
        REQUIRE(game.potato().has_value());
        CHECK(game.potato()->ticks_left == 1);
        REQUIRE(game.tick() == TickOutcome::MOVED);
        CHECK_FALSE(game.potato().has_value());
        CHECK_FALSE(game.lump().has_value());
    }
    {
        Simulation::Setup setup = potato_setup(snake, {1, 1});
        setup.potato = Simulation::Potato{.cell = {9, 8}, .ticks_left = 1};
        Simulation game = required(setup);
        CHECK(game.tick() == TickOutcome::ATE_POTATO);
    }
}

TEST_CASE("Food and the bonus item stay off the bomb food", "[potato][properties]")
{
    for (std::uint64_t seed = 1; seed <= 20; ++seed)
    {
        Simulation game(EdgeRule::WALLS, seed, Variant::HOT_POTATO);
        for (int tick = 0; tick < 3000 && game.running(); ++tick)
        {
            static_cast<void>(game.turn(autoplay(game)));
            if (game.lump_index() && *game.lump_index() >= 1)
            {
                static_cast<void>(game.shed());
            }
            static_cast<void>(game.tick());
            if (game.potato())
            {
                REQUIRE(game.potato()->cell != game.food());
                REQUIRE_FALSE(on_snake(game, game.potato()->cell));
                if (game.bonus())
                {
                    REQUIRE(game.potato()->cell != game.bonus()->cell);
                }
                // One bomb at a time.
                REQUIRE_FALSE(game.lump().has_value());
            }
            if (game.lump())
            {
                REQUIRE(on_snake(game, *game.lump()));
            }
        }
    }
}

TEST_CASE("A game that sheds as soon as it can never blows up, and one that never sheds does", "[potato][properties]")
{
    bool exploded_without_shedding = false;
    for (std::uint64_t seed = 1; seed <= 30; ++seed)
    {
        Simulation sheds(EdgeRule::WALLS, seed, Variant::HOT_POTATO);
        Simulation keeps(EdgeRule::WALLS, seed, Variant::HOT_POTATO);
        for (int tick = 0; tick < 4000 && (sheds.running() || keeps.running()); ++tick)
        {
            if (sheds.running())
            {
                static_cast<void>(sheds.turn(autoplay(sheds)));
                static_cast<void>(sheds.shed());
                REQUIRE(sheds.tick() != TickOutcome::EXPLODED);
            }
            if (keeps.running())
            {
                static_cast<void>(keeps.turn(autoplay(keeps)));
                exploded_without_shedding |= keeps.tick() == TickOutcome::EXPLODED;
            }
        }
    }
    CHECK(exploded_without_shedding);
}

TEST_CASE("A game of Hot potato is set up only with pieces that fit", "[potato][setup]")
{
    const std::vector<Cell> snake = row_snake(6);
    const auto from = [&](auto change)
    {
        Simulation::Setup setup = potato_setup(snake, {1, 1});
        change(setup);
        return Simulation::from_setup(setup).has_value();
    };
    CHECK(from([](Simulation::Setup&) {}));
    CHECK(from([&](Simulation::Setup& setup) { setup.lump = snake[2]; }));
    CHECK(from([](Simulation::Setup& setup) { setup.potato = Simulation::Potato{.cell = {15, 15}}; }));
    CHECK(from([](Simulation::Setup& setup) { setup.normal_since_potato = Simulation::POTATO_EVERY; }));

    // A bomb that is not in the body, a bomb food on the snake or the food, both bombs, and a count that is too high.
    CHECK_FALSE(from([](Simulation::Setup& setup) { setup.lump = Cell{15, 15}; }));
    CHECK_FALSE(from([&](Simulation::Setup& setup) { setup.potato = Simulation::Potato{.cell = snake[1]}; }));
    CHECK_FALSE(from([](Simulation::Setup& setup) { setup.potato = Simulation::Potato{.cell = {1, 1}}; }));
    CHECK_FALSE(from(
        [&](Simulation::Setup& setup)
        {
            setup.potato = Simulation::Potato{.cell = {15, 15}};
            setup.lump = snake[2];
        }));
    CHECK_FALSE(from([](Simulation::Setup& setup) { setup.normal_since_potato = Simulation::POTATO_EVERY + 1; }));
    CHECK_FALSE(from(
        [](Simulation::Setup& setup)
        { setup.potato = Simulation::Potato{.cell = {15, 15}, .ticks_left = Simulation::POTATO_LIFETIME_TICKS + 1}; }));
    CHECK_FALSE(
        from([](Simulation::Setup& setup) { setup.potato = Simulation::Potato{.cell = {15, 15}, .ticks_left = 0}; }));

    // Only the variant with bomb food has any.
    CHECK_FALSE(from(
        [](Simulation::Setup& setup)
        {
            setup.variant = Variant::PLAIN;
            setup.potato = Simulation::Potato{.cell = {15, 15}};
        }));
    CHECK_FALSE(from(
        [&](Simulation::Setup& setup)
        {
            setup.variant = Variant::PLAIN;
            setup.lump = snake[2];
        }));
    CHECK_FALSE(from(
        [](Simulation::Setup& setup)
        {
            setup.variant = Variant::PLAIN;
            setup.normal_since_potato = 1;
        }));
}
