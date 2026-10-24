// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/profile/unlocks.hpp"
#include "snake/sim/mode.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace snake::profile;
using snake::sim::Mode;

TEST_CASE("Nothing is unlocked at first", "[unlocks]")
{
    const Unlocks unlocks;
    CHECK(unlocks.none());
    CHECK(unlocks.bits() == 0);
    CHECK_FALSE(unlocks.has(Unlockable::PORTALS));
    CHECK_FALSE(unlocks.has(Unlockable::HOT_POTATO));
}

TEST_CASE("The unlock targets are 2,000 and 3,000 points in Classic", "[unlocks]")
{
    CHECK(unlock_rule(Unlockable::PORTALS).mode == Mode::CLASSIC);
    CHECK(unlock_rule(Unlockable::PORTALS).score == 2000);
    CHECK(unlock_rule(Unlockable::HOT_POTATO).mode == Mode::CLASSIC);
    CHECK(unlock_rule(Unlockable::HOT_POTATO).score == 3000);
}

TEST_CASE("The unlockables are numbered as they are saved", "[unlocks][format]")
{
    CHECK(static_cast<int>(Unlockable::PORTALS) == 0);
    CHECK(static_cast<int>(Unlockable::HOT_POTATO) == 1);
    CHECK(UNLOCKABLE_COUNT == 2);
}

TEST_CASE("A Classic game of 2,000 points opens Portals and no more", "[unlocks]")
{
    Unlocks unlocks;
    const Unlocks opened = unlocks.record_score(Mode::CLASSIC, 2000);
    CHECK(opened.has(Unlockable::PORTALS));
    CHECK_FALSE(opened.has(Unlockable::HOT_POTATO));
    CHECK(unlocks.has(Unlockable::PORTALS));
    CHECK_FALSE(unlocks.has(Unlockable::HOT_POTATO));
    CHECK(unlocks.bits() == 0b01);
}

TEST_CASE("A score one short of the target opens nothing", "[unlocks]")
{
    Unlocks unlocks;
    CHECK(unlocks.record_score(Mode::CLASSIC, 1999).none());
    CHECK(unlocks.none());
}

TEST_CASE("A Classic game of 3,000 points opens both", "[unlocks]")
{
    Unlocks unlocks;
    const Unlocks opened = unlocks.record_score(Mode::CLASSIC, 3000);
    CHECK(opened.has(Unlockable::PORTALS));
    CHECK(opened.has(Unlockable::HOT_POTATO));
    CHECK(unlocks.bits() == 0b11);
}

TEST_CASE("Easy scores do not count", "[unlocks]")
{
    Unlocks unlocks;
    CHECK(unlocks.record_score(Mode::EASY, 999'999).none());
    CHECK(unlocks.none());
}

TEST_CASE("A mode already open is not reported again", "[unlocks]")
{
    Unlocks unlocks;
    REQUIRE_FALSE(unlocks.record_score(Mode::CLASSIC, 2500).none());
    CHECK(unlocks.record_score(Mode::CLASSIC, 2600).none());

    const Unlocks opened = unlocks.record_score(Mode::CLASSIC, 3100);
    CHECK_FALSE(opened.has(Unlockable::PORTALS));
    CHECK(opened.has(Unlockable::HOT_POTATO));
}

TEST_CASE("A lower score later does not close a mode", "[unlocks]")
{
    Unlocks unlocks;
    static_cast<void>(unlocks.record_score(Mode::CLASSIC, 3000));
    static_cast<void>(unlocks.record_score(Mode::CLASSIC, 10));
    CHECK(unlocks.bits() == 0b11);
}

TEST_CASE("Stored bits are restored when they name unlockables", "[unlocks][load]")
{
    Unlocks unlocks;
    CHECK(unlocks.restore(0b10));
    CHECK(unlocks.has(Unlockable::HOT_POTATO));
    CHECK_FALSE(unlocks.has(Unlockable::PORTALS));
    CHECK(unlocks.restore(0));
    CHECK(unlocks.none());
}

TEST_CASE("Stored bits that name no unlockable are refused and change nothing", "[unlocks][load]")
{
    Unlocks unlocks;
    REQUIRE(unlocks.restore(0b01));
    CHECK_FALSE(unlocks.restore(0b100));
    CHECK_FALSE(unlocks.restore(0x8000'0000));
    CHECK(unlocks.bits() == 0b01);
}

TEST_CASE("Unlocks are equal when the same are open", "[unlocks]")
{
    Unlocks first;
    Unlocks second;
    CHECK(first == second);
    static_cast<void>(second.record_score(Mode::CLASSIC, 2000));
    CHECK_FALSE(first == second);
}
