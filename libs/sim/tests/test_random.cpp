// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/random.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>

using snake::sim::Random;

// The expected numbers come from an independent implementation of PCG32 in Python, written from the reference
// algorithm, and for seed 42 and stream 54 they are the values the reference C code prints. They must never change:
// a recorded game depends on them.
TEST_CASE("The generator reproduces the reference sequence", "[random]")
{
    Random random(42, 54);
    constexpr std::array<std::uint32_t, 6> EXPECTED = {0xa15c02b7, 0x7b47f409, 0xba1d3330,
                                                       0x83d2f293, 0xbfa4784b, 0xcbed606e};
    for (const std::uint32_t expected : EXPECTED)
    {
        CHECK(random.next() == expected);
    }
}

TEST_CASE("A second seed and stream give a different pinned sequence", "[random]")
{
    Random random(1, 1);
    constexpr std::array<std::uint32_t, 4> EXPECTED = {0xc9828f91, 0x1592e274, 0xc0262657, 0xa5c2b6d3};
    for (const std::uint32_t expected : EXPECTED)
    {
        CHECK(random.next() == expected);
    }
}

TEST_CASE("The default stream is the reference stream", "[random]")
{
    Random with_default(42);
    Random with_stream(42, 54);
    for (int i = 0; i < 8; ++i)
    {
        CHECK(with_default.next() == with_stream.next());
    }
}

TEST_CASE("The same seed gives the same numbers and different seeds do not", "[random]")
{
    Random first(7);
    Random second(7);
    Random other(8);
    bool differs = false;
    for (int i = 0; i < 16; ++i)
    {
        const std::uint32_t value = first.next();
        CHECK(value == second.next());
        differs = differs || value != other.next();
    }
    CHECK(differs);
}

TEST_CASE("A bounded number is below the bound, and the pinned values hold", "[random]")
{
    Random random(42, 54);
    constexpr std::array<std::uint32_t, 8> EXPECTED = {183, 297, 224, 355, 155, 366, 285, 45};
    for (const std::uint32_t expected : EXPECTED)
    {
        CHECK(random.below(400) == expected);
    }
}

TEST_CASE("A bound of zero or one always gives zero", "[random]")
{
    Random random(3);
    CHECK(random.below(0) == 0);
    for (int i = 0; i < 16; ++i)
    {
        CHECK(random.below(1) == 0);
    }
}

TEST_CASE("Bounded numbers cover the whole range with no value far from its share", "[random]")
{
    Random random(2026);
    constexpr std::uint32_t BOUND = 10;
    constexpr int DRAWS = 100000;
    std::array<int, BOUND> counts{};
    for (int i = 0; i < DRAWS; ++i)
    {
        const std::uint32_t value = random.below(BOUND);
        REQUIRE(value < BOUND);
        ++counts[value];
    }
    // Each value is expected 10000 times. A bias or a stuck generator would be far outside this band.
    for (const int count : counts)
    {
        CHECK(count > 9500);
        CHECK(count < 10500);
    }
}
