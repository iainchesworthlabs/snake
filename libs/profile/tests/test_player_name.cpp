// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/profile/player_name.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string_view>

using snake::profile::PlayerName;

namespace
{

PlayerName name_of(std::string_view text)
{
    const auto name = PlayerName::from_text(text);
    REQUIRE(name.has_value());
    return *name;
}

} // namespace

TEST_CASE("A new name is empty and cannot be saved", "[name]")
{
    const PlayerName name;
    CHECK(name.empty());
    CHECK(name.size() == 0);
    CHECK(name.view().empty());
    CHECK_FALSE(name.can_save());
}

TEST_CASE("Letters are kept in capitals", "[name]")
{
    PlayerName name;
    CHECK(name.append('a'));
    CHECK(name.append('B'));
    CHECK(name.append('z'));
    CHECK(name.view() == "ABZ");
    CHECK(name.size() == 3);
}

TEST_CASE("The characters that are allowed are A to Z, a to z and the space", "[name]")
{
    CHECK(PlayerName::is_allowed('A'));
    CHECK(PlayerName::is_allowed('Z'));
    CHECK(PlayerName::is_allowed('a'));
    CHECK(PlayerName::is_allowed('z'));
    CHECK(PlayerName::is_allowed(' '));
    CHECK_FALSE(PlayerName::is_allowed('@'));
    CHECK_FALSE(PlayerName::is_allowed('['));
    CHECK_FALSE(PlayerName::is_allowed('`'));
    CHECK_FALSE(PlayerName::is_allowed('{'));
    CHECK_FALSE(PlayerName::is_allowed('0'));
    CHECK_FALSE(PlayerName::is_allowed('-'));
    CHECK_FALSE(PlayerName::is_allowed('\0'));
    CHECK_FALSE(PlayerName::is_allowed('\n'));
    CHECK_FALSE(PlayerName::is_allowed(static_cast<char>(0xC3)));
}

TEST_CASE("A character that is not allowed is refused and the name stays as it was", "[name]")
{
    PlayerName name = name_of("AB");
    CHECK_FALSE(name.append('7'));
    CHECK_FALSE(name.append('!'));
    CHECK(name.view() == "AB");
}

TEST_CASE("A name holds at most 12 characters", "[name]")
{
    PlayerName name;
    for (int count = 0; count < 12; ++count)
    {
        CHECK(name.append('X'));
    }
    CHECK(name.size() == PlayerName::MAX_LENGTH);
    CHECK_FALSE(name.append('Y'));
    CHECK(name.view() == "XXXXXXXXXXXX");
}

TEST_CASE("remove_last takes characters off one at a time and stops at empty", "[name]")
{
    PlayerName name = name_of("AB");
    name.remove_last();
    CHECK(name.view() == "A");
    name.remove_last();
    CHECK(name.empty());
    name.remove_last();
    CHECK(name.empty());
}

TEST_CASE("A removed character leaves nothing behind", "[name]")
{
    PlayerName name = name_of("ABC");
    name.remove_last();
    name.remove_last();
    name.remove_last();
    CHECK(name == PlayerName{});
}

TEST_CASE("clear empties the name", "[name]")
{
    PlayerName name = name_of("IAIN");
    name.clear();
    CHECK(name.empty());
}

TEST_CASE("from_text accepts a full name and makes capitals", "[name]")
{
    CHECK(name_of("Mary Jane").view() == "MARY JANE");
    CHECK(name_of("").empty());
    CHECK(name_of("ABCDEFGHIJKL").size() == 12);
}

TEST_CASE("from_text refuses text that is too long or has a character that is not allowed", "[name]")
{
    CHECK_FALSE(PlayerName::from_text("ABCDEFGHIJKLM").has_value());
    CHECK_FALSE(PlayerName::from_text("R2D2").has_value());
    CHECK_FALSE(PlayerName::from_text("MARY-JANE").has_value());
}

TEST_CASE("Save is possible once the name has a letter", "[name][save]")
{
    CHECK_FALSE(name_of("").can_save());
    CHECK_FALSE(name_of(" ").can_save());
    CHECK_FALSE(name_of("            ").can_save());
    CHECK(name_of("A").can_save());
    CHECK(name_of("  A").can_save());
    CHECK(name_of("A B").can_save());
}

TEST_CASE("The saved form has no space at either end", "[name][save]")
{
    CHECK(name_of("  mary jane ").saved()->view() == "MARY JANE");
    CHECK(name_of("A").saved()->view() == "A");
    CHECK(name_of("A  B").saved()->view() == "A  B");
    CHECK(name_of("   Z").saved()->view() == "Z");
}

TEST_CASE("A name with no letter has no saved form", "[name][save]")
{
    CHECK_FALSE(name_of("").saved().has_value());
    CHECK_FALSE(name_of("   ").saved().has_value());
}

TEST_CASE("is_saved_form is true only for a name that saved() would not change", "[name][save]")
{
    CHECK(name_of("MARY JANE").is_saved_form());
    CHECK(name_of("A").is_saved_form());
    CHECK_FALSE(name_of("").is_saved_form());
    CHECK_FALSE(name_of(" ").is_saved_form());
    CHECK_FALSE(name_of(" A").is_saved_form());
    CHECK_FALSE(name_of("A ").is_saved_form());
    CHECK_FALSE(name_of(" A ").is_saved_form());
}

TEST_CASE("Names are equal when their characters are", "[name]")
{
    CHECK(name_of("ann") == name_of("ANN"));
    CHECK_FALSE(name_of("ann") == name_of("anna"));
    CHECK_FALSE(name_of("ann") == name_of("anm"));
}
