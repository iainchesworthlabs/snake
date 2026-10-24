// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/input/swipe_recogniser.hpp"
#include "snake/sim/direction.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <optional>

using namespace snake::input;
using snake::sim::Direction;

TEST_CASE("A tap gives no direction", "[swipe]")
{
    SwipeRecogniser swipe;
    swipe.press(1, {100.0F, 100.0F});
    CHECK_FALSE(swipe.move(1, {103.0F, 98.0F}));
    swipe.release(1);
    CHECK_FALSE(swipe.tracking());
}

TEST_CASE("A swipe gives the direction of the finger's travel", "[swipe]")
{
    struct Case
    {
        TouchPoint to;
        Direction expected;
    };
    // The screen's y grows downwards, so a swipe up moves to a smaller y.
    for (const Case& c : {Case{{160.0F, 100.0F}, Direction::RIGHT}, Case{{40.0F, 100.0F}, Direction::LEFT},
                          Case{{100.0F, 40.0F}, Direction::UP}, Case{{100.0F, 160.0F}, Direction::DOWN}})
    {
        CAPTURE(c.to.x, c.to.y);
        SwipeRecogniser swipe;
        swipe.press(1, {100.0F, 100.0F});
        CHECK(swipe.move(1, c.to) == c.expected);
    }
}

TEST_CASE("A swipe is given as soon as the finger has gone far enough", "[swipe]")
{
    SwipeRecogniser swipe;
    swipe.press(1, {100.0F, 100.0F});
    CHECK_FALSE(swipe.move(1, {123.9F, 100.0F}));
    CHECK(swipe.move(1, {124.0F, 100.0F}) == Direction::RIGHT);
    // The finger has not lifted.
    CHECK(swipe.tracking());
}

TEST_CASE("A slow drag in small steps adds up to a swipe", "[swipe]")
{
    SwipeRecogniser swipe;
    swipe.press(1, {100.0F, 100.0F});
    std::optional<Direction> given;
    int steps = 0;
    for (float y = 95.0F; !given && steps < 20; y -= 5.0F)
    {
        given = swipe.move(1, {100.0F, y});
        ++steps;
    }
    CHECK(given == Direction::UP);
    CHECK(steps == 5);
}

TEST_CASE("The larger movement decides, and a tie goes to the horizontal", "[swipe]")
{
    SwipeRecogniser swipe;
    swipe.press(1, {100.0F, 100.0F});
    CHECK(swipe.move(1, {130.0F, 180.0F}) == Direction::DOWN);

    swipe.press(1, {100.0F, 100.0F});
    CHECK(swipe.move(1, {40.0F, 90.0F}) == Direction::LEFT);

    swipe.press(1, {100.0F, 100.0F});
    CHECK(swipe.move(1, {130.0F, 70.0F}) == Direction::RIGHT);
}

TEST_CASE("A small sideways drift during a swipe does not matter", "[swipe]")
{
    SwipeRecogniser swipe;
    swipe.press(1, {100.0F, 100.0F});
    CHECK_FALSE(swipe.move(1, {110.0F, 90.0F}));
    CHECK(swipe.move(1, {120.0F, 70.0F}) == Direction::UP);
}

TEST_CASE("The finger can carry on in a new direction without lifting", "[swipe]")
{
    SwipeRecogniser swipe;
    swipe.press(1, {100.0F, 100.0F});
    CHECK(swipe.move(1, {130.0F, 100.0F}) == Direction::RIGHT);
    // Measured from where the last swipe was given: this is 30 down, not 30 right and 30 down.
    CHECK(swipe.move(1, {132.0F, 130.0F}) == Direction::DOWN);
    CHECK_FALSE(swipe.move(1, {132.0F, 140.0F}));
    CHECK(swipe.move(1, {132.0F, 154.0F}) == Direction::DOWN);
}

TEST_CASE("Only the first finger is followed", "[swipe]")
{
    SwipeRecogniser swipe;
    swipe.press(1, {100.0F, 100.0F});
    swipe.press(2, {300.0F, 300.0F});
    CHECK_FALSE(swipe.move(2, {400.0F, 300.0F}));
    swipe.release(2);
    CHECK(swipe.tracking());
    CHECK(swipe.move(1, {160.0F, 100.0F}) == Direction::RIGHT);
}

TEST_CASE("After the finger lifts the next one is followed", "[swipe]")
{
    SwipeRecogniser swipe;
    swipe.press(1, {100.0F, 100.0F});
    swipe.release(1);
    CHECK_FALSE(swipe.tracking());
    CHECK_FALSE(swipe.move(1, {200.0F, 100.0F}));
    swipe.press(2, {100.0F, 100.0F});
    CHECK(swipe.move(2, {100.0F, 160.0F}) == Direction::DOWN);
}

TEST_CASE("A second press of the followed finger starts again", "[swipe]")
{
    SwipeRecogniser swipe;
    swipe.press(1, {100.0F, 100.0F});
    swipe.press(1, {300.0F, 300.0F});
    CHECK_FALSE(swipe.move(1, {310.0F, 300.0F}));
    CHECK(swipe.move(1, {360.0F, 300.0F}) == Direction::RIGHT);
}

TEST_CASE("Cancelling stops following the finger", "[swipe]")
{
    SwipeRecogniser swipe;
    swipe.press(1, {100.0F, 100.0F});
    swipe.cancel();
    CHECK_FALSE(swipe.tracking());
    CHECK_FALSE(swipe.move(1, {200.0F, 100.0F}));
}

TEST_CASE("A move with no finger down gives nothing", "[swipe]")
{
    SwipeRecogniser swipe;
    CHECK_FALSE(swipe.tracking());
    CHECK_FALSE(swipe.move(1, {500.0F, 0.0F}));
    swipe.release(1);
    CHECK_FALSE(swipe.tracking());
}

TEST_CASE("The distance can be changed", "[swipe]")
{
    SwipeRecogniser swipe(SwipeConfig{.min_distance = 50.0F});
    swipe.press(1, {0.0F, 0.0F});
    CHECK_FALSE(swipe.move(1, {40.0F, 0.0F}));
    CHECK(swipe.move(1, {50.0F, 0.0F}) == Direction::RIGHT);
}
