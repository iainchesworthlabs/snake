// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/input/action.hpp"
#include "snake/input/key_map.hpp"
#include "snake/sim/direction.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace snake::input;
using snake::sim::Direction;

TEST_CASE("Arrow keys steer", "[keys]")
{
    CHECK(steer_for(Key::ARROW_UP) == Direction::UP);
    CHECK(steer_for(Key::ARROW_DOWN) == Direction::DOWN);
    CHECK(steer_for(Key::ARROW_LEFT) == Direction::LEFT);
    CHECK(steer_for(Key::ARROW_RIGHT) == Direction::RIGHT);
}

TEST_CASE("W A S D steer", "[keys]")
{
    CHECK(steer_for(Key::W) == Direction::UP);
    CHECK(steer_for(Key::A) == Direction::LEFT);
    CHECK(steer_for(Key::S) == Direction::DOWN);
    CHECK(steer_for(Key::D) == Direction::RIGHT);
}

TEST_CASE("Space pauses and resumes, Escape opens the menu and Enter confirms", "[keys]")
{
    CHECK(action_for(Key::SPACE) == Action::PAUSE_RESUME);
    CHECK(action_for(Key::ESCAPE) == Action::MENU);
    CHECK(action_for(Key::ENTER) == Action::CONFIRM);
}

TEST_CASE("Every key either steers or acts, never both", "[keys]")
{
    constexpr Key ALL[] = {Key::ARROW_UP, Key::ARROW_DOWN, Key::ARROW_LEFT, Key::ARROW_RIGHT, Key::W,    Key::A,
                           Key::S,        Key::D,          Key::SPACE,      Key::ESCAPE,      Key::ENTER};
    for (const Key key : ALL)
    {
        CAPTURE(static_cast<int>(key));
        CHECK(steer_for(key).has_value() != action_for(key).has_value());
    }
    CHECK(static_cast<int>(Key::ENTER) == 10);
}
