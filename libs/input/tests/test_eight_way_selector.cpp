// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/input/eight_way_selector.hpp"
#include "snake/sim/direction.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <optional>

using namespace snake::input;
using snake::sim::Direction;

namespace
{

/// The knob `length` of the way out, `degrees` anticlockwise from east (so 90 is straight up).
std::optional<EightWaySelection> point(EightWaySelector& selector, float length, float degrees, Direction heading)
{
    const float radians = degrees * std::numbers::pi_v<float> / 180.0F;
    return selector.update(length * std::cos(radians), length * std::sin(radians), heading);
}

std::optional<Direction> direction_of(const std::optional<EightWaySelection>& selection)
{
    return selection ? std::optional<Direction>(selection->direction) : std::nullopt;
}

} // namespace

TEST_CASE("The middle of the joystick steers nothing", "[eight-way]")
{
    EightWaySelector selector(TOUCH_JOYSTICK);
    CHECK_FALSE(selector.update(0.0F, 0.0F, Direction::RIGHT));
    CHECK_FALSE(selector.update(0.1F, 0.1F, Direction::RIGHT));
    CHECK_FALSE(selector.update(0.0F, 0.29F, Direction::RIGHT));
}

TEST_CASE("Straight out steers that way, whatever the snake is doing", "[eight-way]")
{
    for (const Direction heading : {Direction::UP, Direction::DOWN, Direction::LEFT, Direction::RIGHT})
    {
        EightWaySelector selector(TOUCH_JOYSTICK);
        CHECK(direction_of(point(selector, 0.8F, 0.0F, heading)) == Direction::RIGHT);
        CHECK(direction_of(point(selector, 0.8F, 90.0F, heading)) == Direction::UP);
        CHECK(direction_of(point(selector, 0.8F, 180.0F, heading)) == Direction::LEFT);
        CHECK(direction_of(point(selector, 0.8F, 270.0F, heading)) == Direction::DOWN);
        CHECK_FALSE(point(selector, 0.8F, 0.0F, heading)->diagonal);
    }
}

TEST_CASE("A diagonal asks for the turn: the way across the snake's own line", "[eight-way]")
{
    EightWaySelector selector(TOUCH_JOYSTICK);
    // North-east, 45 degrees.
    CHECK(direction_of(point(selector, 0.8F, 45.0F, Direction::RIGHT)) == Direction::UP);
    CHECK(direction_of(point(selector, 0.8F, 45.0F, Direction::LEFT)) == Direction::UP);
    CHECK(direction_of(point(selector, 0.8F, 45.0F, Direction::UP)) == Direction::RIGHT);
    CHECK(direction_of(point(selector, 0.8F, 45.0F, Direction::DOWN)) == Direction::RIGHT);
    // South-west.
    CHECK(direction_of(point(selector, 0.8F, 225.0F, Direction::RIGHT)) == Direction::DOWN);
    CHECK(direction_of(point(selector, 0.8F, 225.0F, Direction::UP)) == Direction::LEFT);
    // North-west and south-east.
    CHECK(direction_of(point(selector, 0.8F, 135.0F, Direction::RIGHT)) == Direction::UP);
    CHECK(direction_of(point(selector, 0.8F, 135.0F, Direction::DOWN)) == Direction::LEFT);
    CHECK(direction_of(point(selector, 0.8F, 315.0F, Direction::LEFT)) == Direction::DOWN);
    CHECK(direction_of(point(selector, 0.8F, 315.0F, Direction::UP)) == Direction::RIGHT);
    CHECK(point(selector, 0.8F, 45.0F, Direction::RIGHT)->diagonal);
}

TEST_CASE("A thumb held on a diagonal steers a staircase as the heading changes under it", "[eight-way]")
{
    EightWaySelector selector(TOUCH_JOYSTICK);
    Direction heading = Direction::RIGHT;
    for (int step = 0; step < 4; ++step)
    {
        const auto selection = point(selector, 0.8F, 45.0F, heading);
        REQUIRE(selection);
        // Alternately up and right.
        CHECK(selection->direction == (step % 2 == 0 ? Direction::UP : Direction::RIGHT));
        heading = selection->direction;
    }
}

TEST_CASE("A flick to the diagonal and back is one row aside", "[eight-way]")
{
    EightWaySelector selector(TOUCH_JOYSTICK);
    // Heading east, the thumb at east, then north-east, then east again: up, then right.
    CHECK(direction_of(point(selector, 0.8F, 0.0F, Direction::RIGHT)) == Direction::RIGHT);
    CHECK(direction_of(point(selector, 0.8F, 45.0F, Direction::RIGHT)) == Direction::UP);
    // The caller gives the heading after the queued turn: up. The thumb back at east asks for right.
    CHECK(direction_of(point(selector, 0.8F, 0.0F, Direction::UP)) == Direction::RIGHT);
}

TEST_CASE("The diagonals are 40 degrees wide and the straight ones 50", "[eight-way]")
{
    // Coming from a straight direction the diagonal starts at 25 degrees off the axis.
    EightWaySelector selector(TOUCH_JOYSTICK);
    CHECK_FALSE(point(selector, 0.8F, 20.0F, Direction::RIGHT)->diagonal);
    CHECK_FALSE(point(selector, 0.8F, 24.0F, Direction::RIGHT)->diagonal);
    CHECK(point(selector, 0.8F, 26.0F, Direction::RIGHT)->diagonal);
    // From the other side it is the same: 65 degrees is 25 off the vertical axis, and a thumb that comes from straight
    // up is straight until then.
    EightWaySelector from_up(TOUCH_JOYSTICK);
    CHECK_FALSE(point(from_up, 0.8F, 86.0F, Direction::RIGHT)->diagonal);
    CHECK_FALSE(point(from_up, 0.8F, 68.0F, Direction::RIGHT)->diagonal);
    CHECK(point(from_up, 0.8F, 64.0F, Direction::RIGHT)->diagonal);
    // Having reached the diagonal, it holds a little longer on the way back: until 20 degrees off the axis, which
    // is 70.
    CHECK(point(from_up, 0.8F, 68.0F, Direction::RIGHT)->diagonal);
    CHECK_FALSE(point(from_up, 0.8F, 72.0F, Direction::RIGHT)->diagonal);
}

TEST_CASE("On the edge of a diagonal the choice holds until the thumb is clearly out", "[eight-way]")
{
    EightWaySelector selector(TOUCH_JOYSTICK);
    CHECK(point(selector, 0.8F, 30.0F, Direction::RIGHT)->diagonal);
    // 22 degrees is under the 25 that starts a diagonal and over the 20 that ends it: it stays a diagonal.
    CHECK(point(selector, 0.8F, 22.0F, Direction::RIGHT)->diagonal);
    CHECK_FALSE(point(selector, 0.8F, 18.0F, Direction::RIGHT)->diagonal);
    // Back at 22 it stays straight, until 25.
    CHECK_FALSE(point(selector, 0.8F, 22.0F, Direction::RIGHT)->diagonal);
}

TEST_CASE("The knob engages at 30 percent and lets go under 20", "[eight-way]")
{
    EightWaySelector selector(TOUCH_JOYSTICK);
    CHECK_FALSE(point(selector, 0.28F, 90.0F, Direction::RIGHT));
    CHECK(direction_of(point(selector, 0.31F, 90.0F, Direction::RIGHT)) == Direction::UP);
    // Once steering it holds down to 20 percent.
    CHECK(direction_of(point(selector, 0.22F, 90.0F, Direction::RIGHT)) == Direction::UP);
    CHECK_FALSE(point(selector, 0.18F, 90.0F, Direction::RIGHT));
    // Let go, it needs 30 again.
    CHECK_FALSE(point(selector, 0.25F, 90.0F, Direction::RIGHT));
}

TEST_CASE("Resetting forgets the thumb", "[eight-way]")
{
    EightWaySelector selector(TOUCH_JOYSTICK);
    CHECK(point(selector, 0.8F, 30.0F, Direction::RIGHT)->diagonal);
    selector.reset();
    // Fresh: 22 degrees is a straight direction again.
    CHECK_FALSE(point(selector, 0.8F, 22.0F, Direction::RIGHT)->diagonal);
    CHECK(selector.config().engage == TOUCH_JOYSTICK.engage);
}

TEST_CASE("A tie between the axes on a straight reading goes to the horizontal one", "[eight-way]")
{
    // With a diagonal that starts above ratio 1 the tie is straight.
    EightWaySelector selector({.engage = 0.3F, .release = 0.2F, .diagonal_enter = 2.0F, .diagonal_leave = 1.5F});
    CHECK(direction_of(selector.update(0.5F, 0.5F, Direction::UP)) == Direction::RIGHT);
    CHECK(direction_of(selector.update(-0.5F, 0.5F, Direction::UP)) == Direction::LEFT);
}
