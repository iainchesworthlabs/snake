// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/cell.hpp"
#include "snake/sim/occupancy_set.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>

using snake::sim::OccupancySet;

namespace
{
constexpr std::size_t third_clear_index()
{
    OccupancySet<10> set;
    set.set(0);
    set.set(2);
    return set.nth_clear(2);
}
// Members 0 and 2 are taken, so the clear indices are 1, 3, 4 and the third is 4.
static_assert(third_clear_index() == 4);
} // namespace

TEST_CASE("A new occupancy set has no members", "[occupancy_set]")
{
    OccupancySet<400> set;
    CHECK(set.count() == 0);
    CHECK_FALSE(set.test(0));
    CHECK_FALSE(set.test(399));
}

TEST_CASE("Setting and resetting a bit changes only that bit", "[occupancy_set]")
{
    OccupancySet<400> set;
    set.set(63);
    set.set(64);
    CHECK(set.test(63));
    CHECK(set.test(64));
    CHECK_FALSE(set.test(62));
    CHECK_FALSE(set.test(65));
    CHECK(set.count() == 2);
    set.reset(63);
    CHECK_FALSE(set.test(63));
    CHECK(set.test(64));
    CHECK(set.count() == 1);
}

TEST_CASE("Indices past the end are ignored", "[occupancy_set]")
{
    OccupancySet<400> set;
    set.set(400);
    set.set(1000);
    CHECK(set.count() == 0);
    CHECK_FALSE(set.test(400));
    set.reset(400);
    CHECK(set.count() == 0);
}

TEST_CASE("Clearing removes every member", "[occupancy_set]")
{
    OccupancySet<400> set;
    for (std::size_t index = 0; index < 400; index += 7)
    {
        set.set(index);
    }
    set.clear();
    CHECK(set.count() == 0);
}

TEST_CASE("The nth clear index skips members, across word boundaries", "[occupancy_set]")
{
    OccupancySet<400> set;
    set.set(0);
    set.set(1);
    set.set(64);
    set.set(65);
    CHECK(set.nth_clear(0) == 2);
    CHECK(set.nth_clear(1) == 3);
    CHECK(set.nth_clear(61) == 63);
    CHECK(set.nth_clear(62) == 66);
    CHECK(set.count() == 4);
}

TEST_CASE("The nth clear index of an empty set is n", "[occupancy_set]")
{
    OccupancySet<400> set;
    for (std::size_t rank = 0; rank < 400; ++rank)
    {
        REQUIRE(set.nth_clear(rank) == rank);
    }
}

TEST_CASE("The nth clear index matches a plain scan for every rank", "[occupancy_set]")
{
    OccupancySet<400> set;
    for (std::size_t index = 0; index < 400; ++index)
    {
        if (index % 3 == 0 || index % 17 == 0)
        {
            set.set(index);
        }
    }
    std::size_t rank = 0;
    for (std::size_t index = 0; index < 400; ++index)
    {
        if (!set.test(index))
        {
            REQUIRE(set.nth_clear(rank) == index);
            ++rank;
        }
    }
    CHECK(rank == 400 - set.count());
}

TEST_CASE("A rank beyond the clear indices returns the size of the set", "[occupancy_set]")
{
    OccupancySet<400> set;
    for (std::size_t index = 0; index < 399; ++index)
    {
        set.set(index);
    }
    CHECK(set.nth_clear(0) == 399);
    CHECK(set.nth_clear(1) == 400);
}

TEST_CASE("The bits past the end of the last word are never counted as clear", "[occupancy_set]")
{
    OccupancySet<10> set;
    for (std::size_t index = 0; index < 10; ++index)
    {
        set.set(index);
    }
    CHECK(set.nth_clear(0) == 10);
}

TEST_CASE("A full board has no clear cell", "[occupancy_set]")
{
    OccupancySet<snake::sim::BOARD_CELLS> set;
    for (std::size_t index = 0; index < snake::sim::BOARD_CELLS; ++index)
    {
        set.set(index);
    }
    CHECK(set.count() == snake::sim::BOARD_CELLS);
    CHECK(set.nth_clear(0) == snake::sim::BOARD_CELLS);
}
