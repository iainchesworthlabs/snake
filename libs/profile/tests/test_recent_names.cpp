// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/profile/player_name.hpp"
#include "snake/profile/recent_names.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string_view>

using snake::profile::PlayerName;
using snake::profile::RecentNames;

namespace
{

PlayerName name_of(std::string_view text)
{
    const auto name = PlayerName::from_text(text);
    REQUIRE(name.has_value());
    return *name;
}

} // namespace

TEST_CASE("Recent names start empty", "[recent]")
{
    const RecentNames recent;
    CHECK(recent.empty());
    CHECK(recent.size() == 0);
    CHECK(recent.names().empty());
}

TEST_CASE("The newest name comes first", "[recent]")
{
    RecentNames recent;
    recent.remember(name_of("ANN"));
    recent.remember(name_of("BOB"));
    REQUIRE(recent.size() == 2);
    CHECK(recent.at(0).view() == "BOB");
    CHECK(recent.at(1).view() == "ANN");
}

TEST_CASE("Only the three most recent names are kept", "[recent]")
{
    RecentNames recent;
    recent.remember(name_of("ONE"));
    recent.remember(name_of("TWO"));
    recent.remember(name_of("THREE"));
    recent.remember(name_of("FOUR"));
    REQUIRE(recent.size() == 3);
    CHECK(recent.at(0).view() == "FOUR");
    CHECK(recent.at(1).view() == "THREE");
    CHECK(recent.at(2).view() == "TWO");
}

TEST_CASE("Using a name again moves it to the front without a second copy", "[recent]")
{
    RecentNames recent;
    recent.remember(name_of("ANN"));
    recent.remember(name_of("BOB"));
    recent.remember(name_of("CAL"));
    recent.remember(name_of("ANN"));
    REQUIRE(recent.size() == 3);
    CHECK(recent.at(0).view() == "ANN");
    CHECK(recent.at(1).view() == "CAL");
    CHECK(recent.at(2).view() == "BOB");
}

TEST_CASE("Using the newest name again changes nothing", "[recent]")
{
    RecentNames recent;
    recent.remember(name_of("ANN"));
    recent.remember(name_of("BOB"));
    recent.remember(name_of("BOB"));
    REQUIRE(recent.size() == 2);
    CHECK(recent.at(0).view() == "BOB");
    CHECK(recent.at(1).view() == "ANN");
}

TEST_CASE("Using the middle name again moves it to the front in a list that is not full", "[recent]")
{
    RecentNames recent;
    recent.remember(name_of("ANN"));
    recent.remember(name_of("BOB"));
    recent.remember(name_of("ANN"));
    REQUIRE(recent.size() == 2);
    CHECK(recent.at(0).view() == "ANN");
    CHECK(recent.at(1).view() == "BOB");
}

TEST_CASE("A name is remembered in its saved form", "[recent]")
{
    RecentNames recent;
    recent.remember(name_of("  ann "));
    recent.remember(name_of("ANN"));
    REQUIRE(recent.size() == 1);
    CHECK(recent.at(0).view() == "ANN");
}

TEST_CASE("A name with no letter is not remembered", "[recent]")
{
    RecentNames recent;
    recent.remember(name_of("   "));
    recent.remember(PlayerName{});
    CHECK(recent.empty());
}

TEST_CASE("clear forgets every name", "[recent]")
{
    RecentNames recent;
    recent.remember(name_of("ANN"));
    recent.clear();
    CHECK(recent.empty());
    CHECK(recent == RecentNames{});
}

TEST_CASE("Recent names are equal when their names and order are", "[recent]")
{
    RecentNames first;
    RecentNames second;
    first.remember(name_of("ANN"));
    second.remember(name_of("ANN"));
    CHECK(first == second);
    first.remember(name_of("BOB"));
    CHECK_FALSE(first == second);
}

TEST_CASE("Recent names can be rebuilt from a stored list", "[recent][load]")
{
    const std::array names{name_of("BOB"), name_of("ANN")};
    const auto recent = RecentNames::from_names(names);
    REQUIRE(recent.has_value());
    REQUIRE(recent->size() == 2);
    CHECK(recent->at(0).view() == "BOB");
    CHECK(recent->at(1).view() == "ANN");
    CHECK(RecentNames::from_names({}).has_value());
}

TEST_CASE("A stored list is refused when it is too long, repeats a name or holds a name that cannot be shown",
          "[recent][load]")
{
    const std::array too_many{name_of("A"), name_of("B"), name_of("C"), name_of("D")};
    CHECK_FALSE(RecentNames::from_names(too_many).has_value());

    const std::array repeated{name_of("A"), name_of("B"), name_of("A")};
    CHECK_FALSE(RecentNames::from_names(repeated).has_value());

    const std::array spaced{name_of("A"), name_of(" B")};
    CHECK_FALSE(RecentNames::from_names(spaced).has_value());

    const std::array blank{name_of("A"), name_of("")};
    CHECK_FALSE(RecentNames::from_names(blank).has_value());
}
