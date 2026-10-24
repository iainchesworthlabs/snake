// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/profile/leaderboard.hpp"
#include "snake/profile/player_name.hpp"
#include "snake/sim/mode.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

using namespace snake::profile;
using snake::sim::Mode;

namespace
{

PlayerName name_of(std::string_view text)
{
    const auto name = PlayerName::from_text(text);
    REQUIRE(name.has_value());
    return *name;
}

LeaderboardEntry entry_of(std::string_view name, std::uint32_t score, std::int64_t unix_seconds = 0)
{
    return LeaderboardEntry{.name = name_of(name), .score = score, .unix_seconds = unix_seconds};
}

/// A full board: scores 1000, 900, ... 100, named after their place.
Leaderboard full_board()
{
    Leaderboard board;
    for (std::uint32_t score = 1000; score >= 100; score -= 100)
    {
        REQUIRE(board.insert(entry_of("PLAYER", score)).has_value());
    }
    REQUIRE(board.size() == Leaderboard::CAPACITY);
    return board;
}

} // namespace

TEST_CASE("A new leaderboard is empty and its best score is zero", "[leaderboard]")
{
    const Leaderboard board;
    CHECK(board.empty());
    CHECK(board.size() == 0);
    CHECK(board.best_score() == 0);
    CHECK(board.entries().empty());
}

TEST_CASE("Entries are kept best first", "[leaderboard]")
{
    Leaderboard board;
    CHECK(board.insert(entry_of("ANN", 300)) == 0);
    CHECK(board.insert(entry_of("BOB", 500)) == 0);
    CHECK(board.insert(entry_of("CAL", 400)) == 1);
    CHECK(board.insert(entry_of("DEE", 100)) == 3);
    REQUIRE(board.size() == 4);
    CHECK(board.at(0).name.view() == "BOB");
    CHECK(board.at(1).name.view() == "CAL");
    CHECK(board.at(2).name.view() == "ANN");
    CHECK(board.at(3).name.view() == "DEE");
    CHECK(board.best_score() == 500);
}

TEST_CASE("An entry keeps its name, score and date", "[leaderboard]")
{
    Leaderboard board;
    REQUIRE(board.insert(entry_of("ANN", 420, 1'700'000'000)).has_value());
    CHECK(board.at(0).name.view() == "ANN");
    CHECK(board.at(0).score == 420);
    CHECK(board.at(0).unix_seconds == 1'700'000'000);
}

TEST_CASE("On a tie the earlier entry ranks higher", "[leaderboard][tie]")
{
    Leaderboard board;
    CHECK(board.insert(entry_of("FIRST", 300, 1)) == 0);
    CHECK(board.insert(entry_of("SECOND", 300, 2)) == 1);
    CHECK(board.insert(entry_of("THIRD", 300, 3)) == 2);
    CHECK(board.insert(entry_of("HIGHER", 400, 4)) == 0);
    CHECK(board.insert(entry_of("LOWER", 200, 5)) == 4);
    REQUIRE(board.size() == 5);
    CHECK(board.at(0).name.view() == "HIGHER");
    CHECK(board.at(1).name.view() == "FIRST");
    CHECK(board.at(2).name.view() == "SECOND");
    CHECK(board.at(3).name.view() == "THIRD");
    CHECK(board.at(4).name.view() == "LOWER");
}

TEST_CASE("A score of zero never gets on the board", "[leaderboard]")
{
    Leaderboard board;
    CHECK_FALSE(board.rank_for(0).has_value());
    CHECK_FALSE(board.insert(entry_of("ANN", 0)).has_value());
    CHECK(board.empty());
}

TEST_CASE("Any score above zero fills an empty slot", "[leaderboard]")
{
    Leaderboard board;
    CHECK(board.rank_for(1) == 0);
    REQUIRE(board.insert(entry_of("ANN", 500)).has_value());
    CHECK(board.rank_for(1) == 1);
    CHECK(board.rank_for(500) == 1);
    CHECK(board.rank_for(501) == 0);
}

TEST_CASE("On a full board a score must beat the 10th entry", "[leaderboard]")
{
    const Leaderboard board = full_board();
    CHECK_FALSE(board.rank_for(99).has_value());
    CHECK_FALSE(board.rank_for(100).has_value());
    CHECK(board.rank_for(101) == 9);
    CHECK(board.rank_for(1000) == 1);
    CHECK(board.rank_for(1001) == 0);
}

TEST_CASE("An entry that does not qualify leaves a full board as it was", "[leaderboard]")
{
    Leaderboard board = full_board();
    const Leaderboard before = board;
    CHECK_FALSE(board.insert(entry_of("LATE", 100)).has_value());
    CHECK(board == before);
}

TEST_CASE("A new entry on a full board pushes the 10th entry off", "[leaderboard]")
{
    Leaderboard board = full_board();
    CHECK(board.insert(entry_of("NEW", 550)) == 5);
    REQUIRE(board.size() == 10);
    CHECK(board.at(4).score == 600);
    CHECK(board.at(5).name.view() == "NEW");
    CHECK(board.at(6).score == 500);
    CHECK(board.at(9).score == 200);
}

TEST_CASE("A new entry that takes the last place on a full board replaces the old 10th", "[leaderboard]")
{
    Leaderboard board = full_board();
    CHECK(board.insert(entry_of("NEW", 150)) == 9);
    CHECK(board.at(9).name.view() == "NEW");
    CHECK(board.at(9).score == 150);
    CHECK(board.at(8).score == 200);
}

TEST_CASE("A new best score goes to the top of a full board", "[leaderboard]")
{
    Leaderboard board = full_board();
    CHECK(board.insert(entry_of("BEST", 5000)) == 0);
    CHECK(board.best_score() == 5000);
    CHECK(board.at(1).score == 1000);
    CHECK(board.at(9).score == 200);
}

TEST_CASE("An entry whose name is not in its saved form is refused", "[leaderboard][name]")
{
    Leaderboard board;
    CHECK_FALSE(board.insert(entry_of("", 100)).has_value());
    CHECK_FALSE(board.insert(entry_of("   ", 100)).has_value());
    CHECK_FALSE(board.insert(entry_of(" ANN", 100)).has_value());
    CHECK_FALSE(board.insert(entry_of("ANN ", 100)).has_value());
    CHECK(board.empty());
}

TEST_CASE("clear removes every entry", "[leaderboard][reset]")
{
    Leaderboard board = full_board();
    board.clear();
    CHECK(board.empty());
    CHECK(board.best_score() == 0);
    CHECK(board == Leaderboard{});
}

TEST_CASE("Boards are equal when their entries are", "[leaderboard]")
{
    Leaderboard first;
    Leaderboard second;
    REQUIRE(first.insert(entry_of("ANN", 100, 1)).has_value());
    REQUIRE(second.insert(entry_of("ANN", 100, 1)).has_value());
    CHECK(first == second);
    REQUIRE(second.insert(entry_of("BOB", 50, 2)).has_value());
    CHECK_FALSE(first == second);
}

TEST_CASE("Entries are equal when the name, the score and the date are", "[leaderboard]")
{
    const LeaderboardEntry entry = entry_of("ANN", 100, 5);
    CHECK(entry == entry_of("ANN", 100, 5));
    CHECK_FALSE(entry == entry_of("BOB", 100, 5));
    CHECK_FALSE(entry == entry_of("ANN", 101, 5));
    CHECK_FALSE(entry == entry_of("ANN", 100, 6));
}

TEST_CASE("A board can be rebuilt from stored entries", "[leaderboard][load]")
{
    const std::array entries{entry_of("ANN", 500, 1), entry_of("BOB", 500, 2), entry_of("CAL", 100, 3)};
    const auto board = Leaderboard::from_entries(entries);
    REQUIRE(board.has_value());
    REQUIRE(board->size() == 3);
    CHECK(board->at(1).name.view() == "BOB");
    CHECK(Leaderboard::from_entries({}).has_value());
}

TEST_CASE("Stored entries are refused when they are out of order, zero, unnamed or too many", "[leaderboard][load]")
{
    const std::array out_of_order{entry_of("ANN", 100), entry_of("BOB", 200)};
    CHECK_FALSE(Leaderboard::from_entries(out_of_order).has_value());

    const std::array zero{entry_of("ANN", 100), entry_of("BOB", 0)};
    CHECK_FALSE(Leaderboard::from_entries(zero).has_value());

    const std::array unnamed{entry_of("ANN", 100), entry_of("", 50)};
    CHECK_FALSE(Leaderboard::from_entries(unnamed).has_value());

    const std::array spaced{entry_of(" ANN", 100)};
    CHECK_FALSE(Leaderboard::from_entries(spaced).has_value());

    const std::vector<LeaderboardEntry> eleven(11, entry_of("ANN", 100));
    CHECK_FALSE(Leaderboard::from_entries(eleven).has_value());
    const std::vector<LeaderboardEntry> ten(10, entry_of("ANN", 100));
    CHECK(Leaderboard::from_entries(ten).has_value());
}

TEST_CASE("Each mode keeps its own leaderboard", "[leaderboard][mode]")
{
    Leaderboards boards;
    REQUIRE(boards.of(Mode::EASY).insert(entry_of("ANN", 300)).has_value());
    CHECK(boards.of(Mode::EASY).best_score() == 300);
    CHECK(boards.of(Mode::CLASSIC).empty());

    const Leaderboards& view = boards;
    CHECK(view.of(Mode::EASY).size() == 1);

    boards.of(Mode::EASY).clear();
    REQUIRE(boards.of(Mode::CLASSIC).insert(entry_of("BOB", 200)).has_value());
    CHECK(boards.of(Mode::EASY).empty());
    CHECK(boards.of(Mode::CLASSIC).size() == 1);
}

TEST_CASE("Leaderboards are equal when every board is", "[leaderboard][mode]")
{
    Leaderboards first;
    Leaderboards second;
    CHECK(first == second);
    REQUIRE(second.of(Mode::CLASSIC).insert(entry_of("ANN", 100)).has_value());
    CHECK_FALSE(first == second);
}

TEST_CASE("Mode indices run from zero and refuse numbers that are not a mode", "[mode]")
{
    CHECK(mode_index(Mode::EASY) == 0);
    CHECK(mode_index(Mode::CLASSIC) == 1);
    CHECK(mode_from_index(0) == Mode::EASY);
    CHECK(mode_from_index(1) == Mode::CLASSIC);
    CHECK_FALSE(mode_from_index(MODE_COUNT).has_value());
    CHECK_FALSE(mode_from_index(255).has_value());
}
