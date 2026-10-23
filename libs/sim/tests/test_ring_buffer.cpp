// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/ring_buffer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

using snake::sim::RingBuffer;

namespace
{
// The buffer works at compile time, which shows it needs no allocation.
constexpr int sum_after_wrapping()
{
    RingBuffer<int, 3> buffer;
    buffer.push_back(1);
    buffer.push_back(2);
    buffer.push_back(3);
    buffer.pop_front();
    buffer.push_back(4);
    return buffer[0] + buffer[1] + buffer[2];
}
static_assert(sum_after_wrapping() == 9);
} // namespace

TEST_CASE("A new ring buffer is empty", "[ring_buffer]")
{
    RingBuffer<int, 4> buffer;
    CHECK(buffer.empty());
    CHECK_FALSE(buffer.full());
    CHECK(buffer.size() == 0);
    CHECK(RingBuffer<int, 4>::capacity() == 4);
}

TEST_CASE("Adding at the front puts the newest element first", "[ring_buffer]")
{
    RingBuffer<int, 4> buffer;
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
    RingBuffer<int, 4> buffer;
    REQUIRE(buffer.push_back(1));
    REQUIRE(buffer.push_back(2));
    CHECK(buffer.front() == 1);
    CHECK(buffer.back() == 2);
}

TEST_CASE("A full ring buffer refuses more and keeps what it has", "[ring_buffer]")
{
    RingBuffer<int, 2> buffer;
    REQUIRE(buffer.push_back(1));
    REQUIRE(buffer.push_back(2));
    CHECK(buffer.full());
    CHECK_FALSE(buffer.push_back(3));
    CHECK_FALSE(buffer.push_front(3));
    CHECK(buffer.size() == 2);
    CHECK(buffer.front() == 1);
    CHECK(buffer.back() == 2);
}

TEST_CASE("An empty ring buffer refuses to remove", "[ring_buffer]")
{
    RingBuffer<int, 2> buffer;
    CHECK_FALSE(buffer.pop_front());
    CHECK_FALSE(buffer.pop_back());
    CHECK(buffer.empty());
}

TEST_CASE("Removing from either end leaves the rest in order", "[ring_buffer]")
{
    RingBuffer<int, 4> buffer;
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
    RingBuffer<std::uint16_t, 4> body;
    // The head, 2, is at the front and the tail, 0, at the back.
    REQUIRE(body.push_back(2));
    REQUIRE(body.push_back(1));
    REQUIRE(body.push_back(0));
    // Moving 100 steps keeps the length at 3 and the newest head in front.
    for (std::uint16_t head = 3; head < 103; ++head)
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
    RingBuffer<int, 2> buffer;
    REQUIRE(buffer.push_back(1));
    REQUIRE(buffer.push_back(2));
    buffer.clear();
    CHECK(buffer.empty());
    CHECK(buffer.push_back(7));
    CHECK(buffer.front() == 7);
}
