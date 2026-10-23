// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/ring_buffer.hpp"

#include <catch2/catch_test_macros.hpp>

using snake::sim::RingBuffer;

namespace
{
// The buffer works at compile time, which shows it needs no allocation.
// The tests use one size, so each branch is counted once.
using Buffer = RingBuffer<int, 4>;

constexpr int sum_after_wrapping()
{
    Buffer buffer;
    buffer.push_back(1);
    buffer.push_back(2);
    buffer.push_back(3);
    buffer.push_back(4);
    buffer.pop_front();
    buffer.push_back(5);
    return buffer[0] + buffer[1] + buffer[2] + buffer[3];
}
static_assert(sum_after_wrapping() == 14);
} // namespace

TEST_CASE("A new ring buffer is empty", "[ring_buffer]")
{
    Buffer buffer;
    CHECK(buffer.empty());
    CHECK_FALSE(buffer.full());
    CHECK(buffer.size() == 0);
    CHECK(Buffer::capacity() == 4);
}

TEST_CASE("Adding at the front puts the newest element first", "[ring_buffer]")
{
    Buffer buffer;
    REQUIRE(buffer.push_front(1));
    REQUIRE(buffer.push_front(2));
    REQUIRE(buffer.push_front(3));
    CHECK(buffer.size() == 3);
    CHECK(buffer.front() == 3);
    CHECK(buffer.back() == 1);
    CHECK(buffer[0] == 3);
    CHECK(buffer[1] == 2);
    CHECK(buffer[2] == 1);
}

TEST_CASE("Adding at the back puts the newest element last", "[ring_buffer]")
{
    Buffer buffer;
    REQUIRE(buffer.push_back(1));
    REQUIRE(buffer.push_back(2));
    CHECK(buffer.front() == 1);
    CHECK(buffer.back() == 2);
}

TEST_CASE("A full ring buffer refuses more and keeps what it has", "[ring_buffer]")
{
    Buffer buffer;
    for (int value = 1; value <= 4; ++value)
    {
        REQUIRE(buffer.push_back(value));
    }
    CHECK(buffer.full());
    CHECK_FALSE(buffer.push_back(5));
    CHECK_FALSE(buffer.push_front(5));
    CHECK(buffer.size() == 4);
    CHECK(buffer.front() == 1);
    CHECK(buffer.back() == 4);
}

TEST_CASE("An empty ring buffer refuses to remove", "[ring_buffer]")
{
    Buffer buffer;
    CHECK_FALSE(buffer.pop_front());
    CHECK_FALSE(buffer.pop_back());
    CHECK(buffer.empty());
}

TEST_CASE("Taking from the front returns the element and removes it", "[ring_buffer]")
{
    Buffer buffer;
    CHECK_FALSE(buffer.take_front().has_value());
    REQUIRE(buffer.push_back(1));
    REQUIRE(buffer.push_back(2));
    CHECK(buffer.take_front() == 1);
    CHECK(buffer.take_front() == 2);
    CHECK_FALSE(buffer.take_front().has_value());
    CHECK(buffer.empty());
}

TEST_CASE("Removing from either end leaves the rest in order", "[ring_buffer]")
{
    Buffer buffer;
    for (int value = 1; value <= 4; ++value)
    {
        REQUIRE(buffer.push_back(value));
    }
    REQUIRE(buffer.pop_front());
    REQUIRE(buffer.pop_back());
    CHECK(buffer.size() == 2);
    CHECK(buffer[0] == 2);
    CHECK(buffer[1] == 3);
}

TEST_CASE("The snake pattern of adding a head and dropping the tail wraps around the storage", "[ring_buffer]")
{
    Buffer body;
    // The head, 2, is at the front and the tail, 0, at the back.
    REQUIRE(body.push_back(2));
    REQUIRE(body.push_back(1));
    REQUIRE(body.push_back(0));
    // Moving 100 steps keeps the length at 3 and the newest head in front.
    for (int head = 3; head < 103; ++head)
    {
        REQUIRE(body.push_front(head));
        REQUIRE(body.pop_back());
        REQUIRE(body.size() == 3);
        REQUIRE(body.front() == head);
        REQUIRE(body[1] == head - 1);
        REQUIRE(body.back() == head - 2);
    }
}

TEST_CASE("Clearing empties the buffer and it can be filled again", "[ring_buffer]")
{
    Buffer buffer;
    REQUIRE(buffer.push_back(1));
    REQUIRE(buffer.push_back(2));
    buffer.clear();
    CHECK(buffer.empty());
    CHECK(buffer.push_back(7));
    CHECK(buffer.front() == 7);
}
