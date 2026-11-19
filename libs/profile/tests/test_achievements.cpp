// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/profile/achievements.hpp"
#include "snake/profile/unlocks.hpp"
#include "snake/sim/cell.hpp"
#include "snake/sim/edge_rule.hpp"
#include "snake/sim/mode.hpp"
#include "snake/sim/scoring.hpp"
#include "snake/sim/simulation.hpp"
#include "snake/sim/tally.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

using namespace snake::profile;
using snake::sim::Mode;

namespace
{

constexpr std::array<Mode, 4> ALL_MODES{Mode::EASY, Mode::CLASSIC, Mode::PORTALS, Mode::HOT_POTATO};

constexpr std::uint32_t bit(Achievement achievement)
{
    return std::uint32_t{1} << static_cast<unsigned>(achievement);
}

snake::sim::Simulation required(const snake::sim::Simulation::Setup& setup)
{
    const std::optional<snake::sim::Simulation> game = snake::sim::Simulation::from_setup(setup);
    REQUIRE(game.has_value());
    return *game;
}

/// What a game has to have done, for building the Tally of it.
struct Played
{
    std::uint16_t blueberries = 0;
    std::uint8_t combo = 1; // the combo after each blueberry; counts only when there is a blueberry
    std::uint16_t foods = 0;
    bool bonus = false;
    bool portal = false;
    bool shed = false;
    bool wrap = false;
};

/// A Tally that has seen `played`, built the way the application builds one: from the outcome of each tick and the
/// Simulation after it. The Simulation is set up with the combo and the food count wanted, and observed as often as
/// needed, which is the quickest way to a game of 1,000 blueberries.
snake::sim::Tally tally_of(const Played& played)
{
    using namespace snake::sim;
    static const std::vector<Cell> SNAKE{{5, 5}, {4, 5}, {3, 5}};
    const Simulation game =
        required({.edges = EdgeRule::WALLS,
                  .heading = Direction::RIGHT,
                  .snake = SNAKE,
                  .food = {10, 10},
                  .seed = 3,
                  .scoring = {.foods_eaten = played.foods, .combo = played.combo, .ticks_since_food = 0}});
    Tally tally;
    if (played.wrap)
    {
        // The head goes over the right edge of a board that wraps and comes out on the left.
        static const std::vector<Cell> EDGE_SNAKE{
            {BOARD_COLUMNS - 1, 5}, {BOARD_COLUMNS - 2, 5}, {BOARD_COLUMNS - 3, 5}};
        Simulation edge = required(
            {.edges = EdgeRule::WRAP, .heading = Direction::RIGHT, .snake = EDGE_SNAKE, .food = {10, 10}, .seed = 3});
        tally.start(edge.head());
        tally.observe(edge.tick(), edge);
        REQUIRE(tally.wrapped());
    }
    else
    {
        tally.start(game.head());
    }
    if (played.blueberries == 0)
    {
        tally.observe(TickOutcome::MOVED, game); // takes the food count in
    }
    for (std::uint16_t count = 0; count < played.blueberries; ++count)
    {
        tally.observe(TickOutcome::ATE, game);
    }
    if (played.bonus)
    {
        tally.observe(TickOutcome::ATE_BONUS, game);
    }
    if (played.portal)
    {
        tally.observe(TickOutcome::TELEPORTED, game);
    }
    if (played.shed)
    {
        tally.note_shed();
    }
    REQUIRE(tally.foods_eaten() == played.foods);
    REQUIRE(tally.top_combo() == (played.blueberries > 0 ? played.combo : 0));
    return tally;
}

/// What a game that has `played` earns, on a fresh set, in `mode` with no unlocks and no best to beat.
Achievements earned_by(const Played& played, Mode mode = Mode::CLASSIC)
{
    Achievements achievements;
    return achievements.note_game(mode, 0, tally_of(played), Unlocks{}, 0);
}

Unlocks unlocks_with(std::uint32_t bits)
{
    Unlocks unlocks;
    REQUIRE(unlocks.restore(bits));
    return unlocks;
}

} // namespace

TEST_CASE("The achievements are numbered as they are saved", "[achievements][format]")
{
    CHECK(static_cast<int>(Achievement::WRAP) == 0);
    CHECK(static_cast<int>(Achievement::BONUS) == 1);
    CHECK(static_cast<int>(Achievement::COMBO) == 2);
    CHECK(static_cast<int>(Achievement::BEST) == 3);
    CHECK(static_cast<int>(Achievement::SPEED) == 4);
    CHECK(static_cast<int>(Achievement::BERRIES_100) == 5);
    CHECK(static_cast<int>(Achievement::BERRIES_1000) == 6);
    CHECK(static_cast<int>(Achievement::PORTALS) == 7);
    CHECK(static_cast<int>(Achievement::PORTAL) == 8);
    CHECK(static_cast<int>(Achievement::HOT_POTATO) == 9);
    CHECK(static_cast<int>(Achievement::SHED) == 10);
    CHECK(static_cast<int>(Achievement::ALL_MODES) == 11);
    CHECK(ACHIEVEMENT_COUNT == 12);
}

TEST_CASE("The counted achievements are 100 and 1,000 blueberries", "[achievements]")
{
    CHECK(BERRIES_100_TARGET == 100);
    CHECK(BERRIES_1000_TARGET == 1000);
}

TEST_CASE("Nothing is earned at first", "[achievements]")
{
    const Achievements achievements;
    CHECK(achievements.none());
    CHECK(achievements.bits() == 0);
    CHECK(achievements.blueberries_eaten() == 0);
    CHECK(achievements.modes_finished() == 0);
    for (std::uint8_t number = 0; number < ACHIEVEMENT_COUNT; ++number)
    {
        CHECK_FALSE(achievements.has(static_cast<Achievement>(number)));
    }
}

TEST_CASE("A game of nothing earns nothing but the mode it was played in", "[achievements]")
{
    Achievements achievements;
    const Achievements earned = achievements.note_game(Mode::PORTALS, 0, tally_of(Played{}), Unlocks{}, 0);
    CHECK(earned.none());
    CHECK(achievements.none());
    CHECK(achievements.modes_finished() == 0b0100);
    CHECK(achievements.blueberries_eaten() == 0);
}

TEST_CASE("Through the wall is earned by a wrap and not without one", "[achievements][wrap]")
{
    CHECK(earned_by(Played{.wrap = true}).bits() == bit(Achievement::WRAP));
    CHECK(earned_by(Played{}).none());
}

TEST_CASE("Bonus item is earned by one bonus item", "[achievements][bonus]")
{
    CHECK(earned_by(Played{.bonus = true}).bits() == bit(Achievement::BONUS));
    CHECK(earned_by(Played{}).none());
}

TEST_CASE("Top combo is earned at a combo of 5 and not at 4", "[achievements][combo]")
{
    CHECK(earned_by(Played{.blueberries = 1, .combo = 4}).none());
    CHECK(earned_by(Played{.blueberries = 1, .combo = 5}).bits() == bit(Achievement::COMBO));
    CHECK(snake::sim::Scoring::MAX_COMBO == 5);
}

TEST_CASE("New best needs a best above zero that the score beats", "[achievements][best]")
{
    const auto earned_with = [](std::uint32_t score, std::uint32_t best_before)
    {
        Achievements achievements;
        return achievements.note_game(Mode::CLASSIC, score, tally_of(Played{}), Unlocks{}, best_before);
    };
    CHECK(earned_with(100, 0).none()); // the first game on a board has no best to beat
    CHECK(earned_with(0, 0).none());
    CHECK(earned_with(99, 100).none());  // short of it
    CHECK(earned_with(100, 100).none()); // equal to it
    CHECK(earned_with(101, 100).bits() == bit(Achievement::BEST));
    CHECK(earned_with(1, 0).none());
    CHECK(earned_with(2, 1).bits() == bit(Achievement::BEST));
}

TEST_CASE("Top speed is earned on the food that brings each mode to its floor", "[achievements][speed]")
{
    // Easy takes 2 ms a food from 180 to 120, so 30 foods; the others take 3 ms a food from 150 to 70, or from 160 to
    // 80, so 27 foods (26 foods leave them 2 ms above the floor).
    struct Edge
    {
        Mode mode;
        std::uint16_t foods;
    };
    for (const Edge edge :
         {Edge{Mode::EASY, 30}, Edge{Mode::CLASSIC, 27}, Edge{Mode::PORTALS, 27}, Edge{Mode::HOT_POTATO, 27}})
    {
        CAPTURE(static_cast<int>(edge.mode));
        const std::uint16_t floor_ms = snake::sim::speed_curve_of(edge.mode).floor_ms;
        REQUIRE(snake::sim::tick_interval_ms(edge.mode, edge.foods) == floor_ms);
        REQUIRE(snake::sim::tick_interval_ms(edge.mode, edge.foods - 1) > floor_ms);

        CHECK(earned_by(Played{.foods = static_cast<std::uint16_t>(edge.foods - 1)}, edge.mode).none());
        CHECK(earned_by(Played{.foods = edge.foods}, edge.mode).bits() == bit(Achievement::SPEED));
        CHECK(earned_by(Played{.foods = static_cast<std::uint16_t>(edge.foods + 1)}, edge.mode).bits() ==
              bit(Achievement::SPEED));
        CHECK(earned_by(Played{.foods = 300}, edge.mode).bits() == bit(Achievement::SPEED));
    }
}

TEST_CASE("Top speed is judged by the mode the game was played in", "[achievements][speed]")
{
    // 27 foods are enough in Classic and not in Easy.
    CHECK(earned_by(Played{.foods = 27}, Mode::CLASSIC).has(Achievement::SPEED));
    CHECK_FALSE(earned_by(Played{.foods = 27}, Mode::EASY).has(Achievement::SPEED));
}

TEST_CASE("100 blueberries are earned over several games, at the hundredth", "[achievements][berries]")
{
    Achievements achievements;
    const snake::sim::Tally forty = tally_of(Played{.blueberries = 40});
    CHECK(achievements.note_game(Mode::CLASSIC, 0, forty, Unlocks{}, 0).none());
    CHECK(achievements.note_game(Mode::EASY, 0, forty, Unlocks{}, 0).none());
    CHECK(achievements.blueberries_eaten() == 80);
    CHECK(achievements.note_game(Mode::CLASSIC, 0, tally_of(Played{.blueberries = 19}), Unlocks{}, 0).none());
    CHECK(achievements.blueberries_eaten() == 99);
    CHECK_FALSE(achievements.has(Achievement::BERRIES_100));

    const Achievements earned =
        achievements.note_game(Mode::CLASSIC, 0, tally_of(Played{.blueberries = 1}), Unlocks{}, 0);
    CHECK(earned.bits() == bit(Achievement::BERRIES_100));
    CHECK(achievements.has(Achievement::BERRIES_100));
    CHECK_FALSE(achievements.has(Achievement::BERRIES_1000));
    CHECK(achievements.blueberries_eaten() == 100);
}

TEST_CASE("1,000 blueberries are earned at the thousandth", "[achievements][berries]")
{
    Achievements achievements;
    REQUIRE(achievements.restore(bit(Achievement::BERRIES_100), 998, 0));
    CHECK(achievements.note_game(Mode::CLASSIC, 0, tally_of(Played{.blueberries = 1}), Unlocks{}, 0).none());
    CHECK(achievements.blueberries_eaten() == 999);
    const Achievements earned =
        achievements.note_game(Mode::CLASSIC, 0, tally_of(Played{.blueberries = 1}), Unlocks{}, 0);
    CHECK(earned.bits() == bit(Achievement::BERRIES_1000));
    CHECK(achievements.has(Achievement::BERRIES_100));
    CHECK(achievements.has(Achievement::BERRIES_1000));
}

TEST_CASE("One big game can earn both counted achievements", "[achievements][berries]")
{
    Achievements achievements;
    const Achievements earned =
        achievements.note_game(Mode::EASY, 0, tally_of(Played{.blueberries = 1000}), Unlocks{}, 0);
    CHECK(earned.bits() == (bit(Achievement::BERRIES_100) | bit(Achievement::BERRIES_1000)));
    CHECK(achievements.blueberries_eaten() == 1000);
}

TEST_CASE("The blueberry count stops at its largest value", "[achievements][berries]")
{
    constexpr std::uint32_t MOST = std::numeric_limits<std::uint32_t>::max();
    Achievements achievements;
    REQUIRE(achievements.restore(0, MOST - 5, 0));
    CHECK(achievements.note_game(Mode::CLASSIC, 0, tally_of(Played{.blueberries = 5}), Unlocks{}, 0)
              .has(Achievement::BERRIES_1000));
    CHECK(achievements.blueberries_eaten() == MOST);
    CHECK(achievements.note_game(Mode::CLASSIC, 0, tally_of(Played{.blueberries = 3}), Unlocks{}, 0).none());
    CHECK(achievements.blueberries_eaten() == MOST);

    REQUIRE(achievements.restore(0, MOST - 5, 0));
    static_cast<void>(achievements.note_game(Mode::CLASSIC, 0, tally_of(Played{.blueberries = 9}), Unlocks{}, 0));
    CHECK(achievements.blueberries_eaten() == MOST);
}

TEST_CASE("Portals open and Hot potato open follow the unlocks after the game", "[achievements][unlocks]")
{
    Achievements achievements;
    const snake::sim::Tally nothing = tally_of(Played{});
    CHECK(achievements.note_game(Mode::CLASSIC, 0, nothing, Unlocks{}, 0).none());
    CHECK(achievements.note_game(Mode::CLASSIC, 0, nothing, unlocks_with(0b01), 0).bits() == bit(Achievement::PORTALS));
    CHECK(achievements.note_game(Mode::CLASSIC, 0, nothing, unlocks_with(0b11), 0).bits() ==
          bit(Achievement::HOT_POTATO));
    CHECK(achievements.has(Achievement::PORTALS));
    CHECK(achievements.has(Achievement::HOT_POTATO));

    Achievements both;
    CHECK(both.note_game(Mode::CLASSIC, 0, nothing, unlocks_with(0b11), 0).bits() ==
          (bit(Achievement::PORTALS) | bit(Achievement::HOT_POTATO)));
    Achievements hot_only;
    CHECK(hot_only.note_game(Mode::CLASSIC, 0, nothing, unlocks_with(0b10), 0).bits() == bit(Achievement::HOT_POTATO));
}

TEST_CASE("Through a portal is earned by a portal", "[achievements][portal]")
{
    CHECK(earned_by(Played{.portal = true}, Mode::PORTALS).bits() == bit(Achievement::PORTAL));
    CHECK(earned_by(Played{}, Mode::PORTALS).none());
}

TEST_CASE("Shed in time is earned by a shed", "[achievements][shed]")
{
    CHECK(earned_by(Played{.shed = true}, Mode::HOT_POTATO).bits() == bit(Achievement::SHED));
    CHECK(earned_by(Played{}, Mode::HOT_POTATO).none());
}

TEST_CASE("All four modes are earned by the fourth mode, in any order", "[achievements][modes]")
{
    const snake::sim::Tally nothing = tally_of(Played{});
    for (std::size_t skipped = 0; skipped < ALL_MODES.size(); ++skipped)
    {
        Achievements achievements;
        for (std::size_t index = 0; index < ALL_MODES.size(); ++index)
        {
            if (index != skipped)
            {
                CHECK(achievements.note_game(ALL_MODES[index], 0, nothing, Unlocks{}, 0).none());
            }
        }
        // Three modes, and the same one twice more, are not four.
        CHECK(achievements.note_game(ALL_MODES[(skipped + 1) % 4], 0, nothing, Unlocks{}, 0).none());
        CHECK_FALSE(achievements.has(Achievement::ALL_MODES));
        CHECK(achievements.modes_finished() == (0b1111 & ~(1U << skipped)));

        const Achievements earned = achievements.note_game(ALL_MODES[skipped], 0, nothing, Unlocks{}, 0);
        CHECK(earned.bits() == bit(Achievement::ALL_MODES));
        CHECK(achievements.modes_finished() == 0b1111);
    }
}

TEST_CASE("A game earns every achievement it meets at once and returns them all", "[achievements]")
{
    Achievements achievements;
    const Played everything{
        .blueberries = 1000, .combo = 5, .foods = 40, .bonus = true, .portal = true, .shed = true, .wrap = true};
    for (const Mode mode : {Mode::EASY, Mode::CLASSIC, Mode::PORTALS})
    {
        static_cast<void>(achievements.note_game(mode, 0, tally_of(Played{}), Unlocks{}, 0));
    }
    const Achievements earned =
        achievements.note_game(Mode::HOT_POTATO, 500, tally_of(everything), unlocks_with(0b11), 400);
    CHECK(earned.bits() == 0xFFF);
    CHECK(achievements.bits() == 0xFFF);
    CHECK(earned.blueberries_eaten() == 0);
    CHECK(earned.modes_finished() == 0);
}

TEST_CASE("The same game again earns nothing new", "[achievements]")
{
    Achievements achievements;
    const snake::sim::Tally tally = tally_of(Played{.blueberries = 100, .combo = 5, .foods = 40, .bonus = true});
    const Unlocks unlocks = unlocks_with(0b01);
    const Achievements first = achievements.note_game(Mode::CLASSIC, 500, tally, unlocks, 400);
    CHECK_FALSE(first.none());
    const std::uint32_t after_first = achievements.bits();
    const Achievements second = achievements.note_game(Mode::CLASSIC, 500, tally, unlocks, 400);
    CHECK(second.none());
    CHECK(second.bits() == 0);
    CHECK(achievements.bits() == after_first);
    CHECK(achievements.blueberries_eaten() == 200);
}

TEST_CASE("An achievement is never taken away", "[achievements]")
{
    Achievements achievements;
    static_cast<void>(achievements.note_game(Mode::CLASSIC, 900, tally_of(Played{.bonus = true, .wrap = true}),
                                             unlocks_with(0b11), 100));
    const std::uint32_t bits = achievements.bits();
    REQUIRE(bits != 0);
    // A poor game with every unlock somehow gone and no best earns nothing and loses nothing.
    CHECK(achievements.note_game(Mode::CLASSIC, 0, tally_of(Played{}), Unlocks{}, 0).none());
    CHECK((achievements.bits() & bits) == bits);
}

TEST_CASE("Restoring takes the three stored values", "[achievements][restore]")
{
    Achievements achievements;
    REQUIRE(achievements.restore(0xFFF, 123, 0b1010));
    CHECK(achievements.bits() == 0xFFF);
    CHECK(achievements.blueberries_eaten() == 123);
    CHECK(achievements.modes_finished() == 0b1010);
    CHECK(achievements.has(Achievement::ALL_MODES));
    CHECK_FALSE(achievements.none());
}

TEST_CASE("Restoring refuses a bit that is not an achievement or not a mode and changes nothing",
          "[achievements][restore]")
{
    Achievements achievements;
    REQUIRE(achievements.restore(0b101, 7, 0b0011));
    const Achievements before = achievements;

    CHECK_FALSE(achievements.restore(1U << ACHIEVEMENT_COUNT, 9, 0));
    CHECK(achievements == before);
    CHECK_FALSE(achievements.restore(0x80000000U, 9, 0));
    CHECK(achievements == before);
    CHECK_FALSE(achievements.restore(0, 9, 0b1'0000));
    CHECK(achievements == before);
    CHECK_FALSE(achievements.restore(0, 9, 0x80));
    CHECK(achievements == before);
    CHECK_FALSE(achievements.restore(1U << ACHIEVEMENT_COUNT, 9, 0b1'0000));
    CHECK(achievements == before);
}

TEST_CASE("Syncing the unlocks sets the two mode achievements and reports what it set", "[achievements][unlocks]")
{
    Achievements achievements;
    CHECK(achievements.sync_unlocks(Unlocks{}).none());
    CHECK(achievements.none());

    CHECK(achievements.sync_unlocks(unlocks_with(0b01)).bits() == bit(Achievement::PORTALS));
    CHECK(achievements.sync_unlocks(unlocks_with(0b01)).none());
    CHECK(achievements.sync_unlocks(unlocks_with(0b11)).bits() == bit(Achievement::HOT_POTATO));
    CHECK(achievements.bits() == (bit(Achievement::PORTALS) | bit(Achievement::HOT_POTATO)));

    Achievements hot_only;
    CHECK(hot_only.sync_unlocks(unlocks_with(0b10)).bits() == bit(Achievement::HOT_POTATO));
    Achievements both;
    CHECK(both.sync_unlocks(unlocks_with(0b11)).bits() == (bit(Achievement::PORTALS) | bit(Achievement::HOT_POTATO)));
}

TEST_CASE("Syncing the unlocks leaves the counts and the other achievements alone", "[achievements][unlocks]")
{
    Achievements achievements;
    REQUIRE(achievements.restore(bit(Achievement::WRAP), 42, 0b0001));
    static_cast<void>(achievements.sync_unlocks(unlocks_with(0b11)));
    CHECK(achievements.bits() == (bit(Achievement::WRAP) | bit(Achievement::PORTALS) | bit(Achievement::HOT_POTATO)));
    CHECK(achievements.blueberries_eaten() == 42);
    CHECK(achievements.modes_finished() == 0b0001);
}

TEST_CASE("Progress is the blueberries so far and the target, for the two counted achievements",
          "[achievements][progress]")
{
    Achievements achievements;
    REQUIRE(achievements.restore(0, 37, 0));
    CHECK(achievements.progress(Achievement::BERRIES_100) == Progress{.current = 37, .target = 100});
    CHECK(achievements.progress(Achievement::BERRIES_1000) == Progress{.current = 37, .target = 1000});

    REQUIRE(achievements.restore(0, 100, 0));
    CHECK(achievements.progress(Achievement::BERRIES_100) == Progress{.current = 100, .target = 100});
    REQUIRE(achievements.restore(0, 5000, 0));
    CHECK(achievements.progress(Achievement::BERRIES_100) == Progress{.current = 100, .target = 100});
    CHECK(achievements.progress(Achievement::BERRIES_1000) == Progress{.current = 1000, .target = 1000});
    REQUIRE(achievements.restore(0, 0, 0));
    CHECK(achievements.progress(Achievement::BERRIES_100) == Progress{.current = 0, .target = 100});
}

TEST_CASE("The other achievements have no progress", "[achievements][progress]")
{
    Achievements achievements;
    REQUIRE(achievements.restore(0, 50, 0));
    for (std::uint8_t number = 0; number < ACHIEVEMENT_COUNT; ++number)
    {
        const auto achievement = static_cast<Achievement>(number);
        if (achievement != Achievement::BERRIES_100 && achievement != Achievement::BERRIES_1000)
        {
            CHECK_FALSE(achievements.progress(achievement).has_value());
        }
    }
}

TEST_CASE("Achievements are equal when the bits, the count and the modes are", "[achievements]")
{
    Achievements base;
    CHECK(base == Achievements{});

    Achievements changed;
    REQUIRE(changed.restore(1, 0, 0));
    CHECK_FALSE(base == changed);
    REQUIRE(changed.restore(0, 1, 0));
    CHECK_FALSE(base == changed);
    REQUIRE(changed.restore(0, 0, 1));
    CHECK_FALSE(base == changed);
    REQUIRE(changed.restore(0, 0, 0));
    CHECK(base == changed);
}
