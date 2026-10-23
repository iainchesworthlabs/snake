// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/input_buffer.hpp"

#include <catch2/catch_test_macros.hpp>

using snake::sim::Direction;
using snake::sim::InputBuffer;

namespace
{
constexpr Direction ALL[] = {Direction::UP, Direction::DOWN, Direction::LEFT, Direction::RIGHT};
}

TEST_CASE("A new input buffer is empty and gives nothing", "[input_buffer]")
{
    InputBuffer buffer;
    CHECK(buffer.empty());
    CHECK(buffer.size() == 0);
    CHECK_FALSE(buffer.pop().has_value());
}

TEST_CASE("A turn to the side is queued and then taken once", "[input_buffer]")
{
    InputBuffer buffer;
    REQUIRE(buffer.push(Direction::RIGHT, Direction::UP));
    CHECK(buffer.size() == 1);
    CHECK(buffer.pop() == Direction::UP);
    CHECK(buffer.empty());
    CHECK_FALSE(buffer.pop().has_value());
}

TEST_CASE("A reversal of the current direction is ignored", "[input_buffer]")
{
    InputBuffer buffer;
    CHECK_FALSE(buffer.push(Direction::RIGHT, Direction::LEFT));
    CHECK(buffer.empty());
}

TEST_CASE("Repeating the current direction is ignored", "[input_buffer]")
{
    InputBuffer buffer;
    CHECK_FALSE(buffer.push(Direction::RIGHT, Direction::RIGHT));
    CHECK(buffer.empty());
}

TEST_CASE("Inputs are taken in the order they arrived, one per call", "[input_buffer]")
{
    InputBuffer buffer;
    REQUIRE(buffer.push(Direction::RIGHT, Direction::UP));
    REQUIRE(buffer.push(Direction::RIGHT, Direction::LEFT));
    CHECK(buffer.pop() == Direction::UP);
    CHECK(buffer.pop() == Direction::LEFT);
    CHECK_FALSE(buffer.pop().has_value());
}

TEST_CASE("A fast up-then-down while moving right cannot reverse the snake", "[input_buffer]")
{
    // Up is accepted. Down would reverse up (the snake will be travelling up by then), so it is dropped. Judged
    // against the current direction, right, down would have been accepted and turned the snake back on itself.
    InputBuffer buffer;
    REQUIRE(buffer.push(Direction::RIGHT, Direction::UP));
    CHECK_FALSE(buffer.push(Direction::RIGHT, Direction::DOWN));
    CHECK(buffer.size() == 1);
}

TEST_CASE("A fast up-then-left while moving right is two valid turns", "[input_buffer]")
{
    InputBuffer buffer;
    REQUIRE(buffer.push(Direction::RIGHT, Direction::UP));
    REQUIRE(buffer.push(Direction::RIGHT, Direction::LEFT));
    CHECK(buffer.size() == 2);
}

TEST_CASE("A repeat of the last queued direction is ignored", "[input_buffer]")
{
    InputBuffer buffer;
    REQUIRE(buffer.push(Direction::RIGHT, Direction::UP));
    CHECK_FALSE(buffer.push(Direction::RIGHT, Direction::UP));
    CHECK(buffer.size() == 1);
}

TEST_CASE("The reversal rule applies to the current direction again once the queue is empty", "[input_buffer]")
{
    InputBuffer buffer;
    REQUIRE(buffer.push(Direction::RIGHT, Direction::UP));
    REQUIRE(buffer.pop() == Direction::UP);
    // The snake now travels up. Down reverses it, and right is a turn.
    CHECK_FALSE(buffer.push(Direction::UP, Direction::DOWN));
    CHECK(buffer.push(Direction::UP, Direction::RIGHT));
}

TEST_CASE("A third input is dropped while two are waiting", "[input_buffer]")
{
    InputBuffer buffer;
    REQUIRE(buffer.push(Direction::RIGHT, Direction::UP));
    REQUIRE(buffer.push(Direction::RIGHT, Direction::LEFT));
    CHECK_FALSE(buffer.push(Direction::RIGHT, Direction::DOWN));
    CHECK(buffer.size() == InputBuffer::CAPACITY);
    CHECK(buffer.pop() == Direction::UP);
    CHECK(buffer.pop() == Direction::LEFT);
}

TEST_CASE("A place frees up after an input is taken", "[input_buffer]")
{
    InputBuffer buffer;
    REQUIRE(buffer.push(Direction::RIGHT, Direction::UP));
    REQUIRE(buffer.push(Direction::RIGHT, Direction::LEFT));
    REQUIRE(buffer.pop() == Direction::UP);
    // Left is still waiting, so the next input is judged against left.
    CHECK_FALSE(buffer.push(Direction::UP, Direction::LEFT));
    CHECK_FALSE(buffer.push(Direction::UP, Direction::RIGHT));
    CHECK(buffer.push(Direction::UP, Direction::DOWN));
    CHECK(buffer.size() == InputBuffer::CAPACITY);
}

TEST_CASE("Clearing drops every waiting input", "[input_buffer]")
{
    InputBuffer buffer;
    REQUIRE(buffer.push(Direction::RIGHT, Direction::UP));
    buffer.clear();
    CHECK(buffer.empty());
    CHECK_FALSE(buffer.pop().has_value());
}

TEST_CASE("With an empty queue exactly the two side turns are accepted, for every heading", "[input_buffer]")
{
    for (const Direction heading : ALL)
    {
        int accepted = 0;
        for (const Direction input : ALL)
        {
            InputBuffer buffer;
            if (buffer.push(heading, input))
            {
                ++accepted;
                REQUIRE(input != heading);
                REQUIRE(input != snake::sim::opposite(heading));
            }
        }
        CHECK(accepted == 2);
    }
}

TEST_CASE("Two queued inputs never contain a reversal or a repeat, whatever arrives", "[input_buffer]")
{
    for (const Direction heading : ALL)
    {
        for (const Direction first : ALL)
        {
            for (const Direction second : ALL)
            {
                InputBuffer buffer;
                buffer.push(heading, first);
                buffer.push(heading, second);
                Direction travelling = heading;
                while (const auto next = buffer.pop())
                {
                    REQUIRE(*next != travelling);
                    REQUIRE(*next != snake::sim::opposite(travelling));
                    travelling = *next;
                }
            }
        }
    }
}
