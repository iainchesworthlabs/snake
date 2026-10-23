// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/scoring.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using snake::sim::score_digits;
using snake::sim::Scoring;

namespace
{

void ticks(Scoring& scoring, int count)
{
    for (int tick = 0; tick < count; ++tick)
    {
        scoring.advance();
    }
}

std::string digits_of(std::uint32_t score)
{
    const auto digits = score_digits(score);
    return std::string(digits.begin(), digits.end());
}

} // namespace

TEST_CASE("A new game has no score, x1 and no foods", "[scoring]")
{
    const Scoring scoring;
    CHECK(scoring.score() == 0);
    CHECK(scoring.combo() == 1);
    CHECK(scoring.foods_eaten() == 0);
}

TEST_CASE("The first food is worth 10 points at x1", "[scoring]")
{
    Scoring scoring;
    scoring.advance();
    const Scoring::FoodResult result = scoring.eat_food();
    CHECK(result.points == 10);
    CHECK_FALSE(result.bonus_due);
    CHECK(scoring.score() == 10);
    CHECK(scoring.combo() == 1);
    CHECK(scoring.foods_eaten() == 1);
}

TEST_CASE("Eating again within the window raises the combo by one each time, up to x5", "[scoring][combo]")
{
    Scoring scoring;
    std::uint32_t expected = 0;
    const std::uint8_t combos[] = {1, 2, 3, 4, 5, 5, 5};
    for (const std::uint8_t combo : combos)
    {
        ticks(scoring, 3);
        const Scoring::FoodResult result = scoring.eat_food();
        expected += 10U * combo;
        CHECK(result.points == 10U * combo);
        CHECK(scoring.combo() == combo);
        CHECK(scoring.score() == expected);
        // The fifth food also owes a bonus item, which these steps leave on the board.
        if (scoring.foods_eaten() % 5 == 0)
        {
            CHECK(result.bonus_due);
        }
    }
}

TEST_CASE("A food on the 20th tick after the last one still counts towards the combo", "[scoring][combo]")
{
    Scoring scoring;
    ticks(scoring, 1);
    static_cast<void>(scoring.eat_food());
    ticks(scoring, 20);
    const Scoring::FoodResult result = scoring.eat_food();
    CHECK(result.points == 20);
    CHECK(scoring.combo() == 2);
}

TEST_CASE("A food on the 21st tick starts the combo again at x1", "[scoring][combo]")
{
    Scoring scoring;
    ticks(scoring, 1);
    static_cast<void>(scoring.eat_food());
    ticks(scoring, 3);
    static_cast<void>(scoring.eat_food());
    REQUIRE(scoring.combo() == 2);
    ticks(scoring, 21);
    const Scoring::FoodResult result = scoring.eat_food();
    CHECK(result.points == 10);
    CHECK(scoring.combo() == 1);
}

TEST_CASE("The live multiplier drops to x1 as soon as the window has passed", "[scoring][combo]")
{
    Scoring scoring;
    ticks(scoring, 1);
    static_cast<void>(scoring.eat_food());
    ticks(scoring, 2);
    static_cast<void>(scoring.eat_food());
    ticks(scoring, 20);
    CHECK(scoring.combo() == 2);
    ticks(scoring, 1);
    CHECK(scoring.combo() == 1);
    ticks(scoring, 500);
    CHECK(scoring.combo() == 1);
    CHECK(scoring.state().ticks_since_food == Scoring::LAPSED);
}

TEST_CASE("Every fifth normal food says a bonus item is due", "[scoring][bonus]")
{
    Scoring scoring;
    for (int food = 1; food <= 15; ++food)
    {
        scoring.advance();
        const Scoring::FoodResult result = scoring.eat_food();
        CHECK(result.bonus_due == (food % 5 == 0));
    }
}

TEST_CASE("Bonus food is worth 50 points whatever the combo", "[scoring][bonus]")
{
    Scoring scoring;
    for (int food = 0; food < 3; ++food)
    {
        scoring.advance();
        static_cast<void>(scoring.eat_food());
    }
    REQUIRE(scoring.combo() == 3);
    const std::uint32_t before = scoring.score();
    scoring.advance();
    CHECK(scoring.eat_bonus() == 50);
    CHECK(scoring.score() == before + 50);
}

TEST_CASE("Bonus food restarts the window and counts for the speed, but keeps the combo where it was",
          "[scoring][bonus]")
{
    Scoring scoring;
    for (int food = 0; food < 2; ++food)
    {
        scoring.advance();
        static_cast<void>(scoring.eat_food());
    }
    REQUIRE(scoring.combo() == 2);
    ticks(scoring, 15);
    static_cast<void>(scoring.eat_bonus());
    CHECK(scoring.combo() == 2);
    CHECK(scoring.foods_eaten() == 3);
    // The window runs from the bonus, not from the last normal food.
    ticks(scoring, 20);
    CHECK(scoring.eat_food().points == 30);
    CHECK(scoring.combo() == 3);
}

TEST_CASE("Bonus food eaten after the window has passed drops the combo to x1", "[scoring][bonus]")
{
    Scoring scoring;
    for (int food = 0; food < 3; ++food)
    {
        scoring.advance();
        static_cast<void>(scoring.eat_food());
    }
    ticks(scoring, 30);
    static_cast<void>(scoring.eat_bonus());
    CHECK(scoring.combo() == 1);
}

TEST_CASE("Bonus food does not count towards the fifth normal food", "[scoring][bonus]")
{
    Scoring scoring;
    for (int food = 1; food <= 4; ++food)
    {
        scoring.advance();
        CHECK_FALSE(scoring.eat_food().bonus_due);
    }
    scoring.advance();
    static_cast<void>(scoring.eat_bonus());
    scoring.advance();
    CHECK(scoring.eat_food().bonus_due);
}

TEST_CASE("The state can be read back and set up again", "[scoring][state]")
{
    Scoring scoring;
    for (int food = 0; food < 7; ++food)
    {
        scoring.advance();
        static_cast<void>(scoring.eat_food());
    }
    const Scoring::State state = scoring.state();
    CHECK(Scoring::valid(state));
    Scoring restored(state);
    CHECK(restored.state() == state);
    ticks(scoring, 2);
    ticks(restored, 2);
    CHECK(restored.eat_food().points == scoring.eat_food().points);
    CHECK(restored.state() == scoring.state());
}

TEST_CASE("A state that cannot happen is not valid", "[scoring][state]")
{
    CHECK(Scoring::valid(Scoring::State{}));
    CHECK_FALSE(Scoring::valid({.combo = 0}));
    CHECK_FALSE(Scoring::valid({.combo = 6}));
    CHECK_FALSE(Scoring::valid({.normal_since_bonus = 5}));
    CHECK_FALSE(Scoring::valid({.ticks_since_food = 22}));
    CHECK(Scoring::valid({.normal_since_bonus = 4, .combo = 5, .ticks_since_food = 0}));
}

TEST_CASE("A score shows as six digits with leading zeros", "[scoring][display]")
{
    CHECK(digits_of(0) == "000000");
    CHECK(digits_of(420) == "000420");
    CHECK(digits_of(7) == "000007");
    CHECK(digits_of(123456) == "123456");
    CHECK(digits_of(999999) == "999999");
}

TEST_CASE("A score above 999999 shows as 999999", "[scoring][display]")
{
    CHECK(digits_of(1'000'000) == "999999");
    CHECK(digits_of(0xFFFFFFFFU) == "999999");
}

TEST_CASE("Two states are equal only when every field is", "[scoring][state]")
{
    const Scoring::State base{};
    CHECK(base == Scoring::State{});
    CHECK_FALSE(base == Scoring::State{.score = 1});
    CHECK_FALSE(base == Scoring::State{.foods_eaten = 1});
    CHECK_FALSE(base == Scoring::State{.normal_since_bonus = 1});
    CHECK_FALSE(base == Scoring::State{.combo = 2});
    CHECK_FALSE(base == Scoring::State{.ticks_since_food = 0});
}
