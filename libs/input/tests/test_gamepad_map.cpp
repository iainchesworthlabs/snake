// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/input/action.hpp"
#include "snake/input/direction_selector.hpp"
#include "snake/input/gamepad_map.hpp"
#include "snake/sim/direction.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace snake::input;
using snake::sim::Direction;

TEST_CASE("The d-pad steers", "[gamepad]")
{
    CHECK(steer_for(GamepadButton::DPAD_UP) == Direction::UP);
    CHECK(steer_for(GamepadButton::DPAD_DOWN) == Direction::DOWN);
    CHECK(steer_for(GamepadButton::DPAD_LEFT) == Direction::LEFT);
    CHECK(steer_for(GamepadButton::DPAD_RIGHT) == Direction::RIGHT);
}

TEST_CASE("Start pauses and resumes, Select and the east button open the menu, the south button confirms", "[gamepad]")
{
    CHECK(action_for(GamepadButton::START) == Action::PAUSE_RESUME);
    CHECK(action_for(GamepadButton::SELECT) == Action::MENU);
    CHECK(action_for(GamepadButton::EAST) == Action::MENU);
    CHECK(action_for(GamepadButton::SOUTH) == Action::CONFIRM);
}

TEST_CASE("Every button either steers or acts, never both", "[gamepad]")
{
    constexpr GamepadButton ALL[] = {GamepadButton::DPAD_UP,    GamepadButton::DPAD_DOWN, GamepadButton::DPAD_LEFT,
                                     GamepadButton::DPAD_RIGHT, GamepadButton::SOUTH,     GamepadButton::EAST,
                                     GamepadButton::START,      GamepadButton::SELECT};
    for (const GamepadButton button : ALL)
    {
        CAPTURE(static_cast<int>(button));
        CHECK(steer_for(button).has_value() != action_for(button).has_value());
    }
}

TEST_CASE("The left stick steers once it is pushed 60 percent of the way", "[gamepad][stick]")
{
    DirectionSelector stick(STICK_SELECTOR);
    CHECK_FALSE(stick.update(0.5F, 0.0F));
    CHECK(stick.update(0.6F, 0.0F) == Direction::RIGHT);
    // It stays set until it comes back under 40 percent.
    CHECK(stick.update(0.45F, 0.0F) == Direction::RIGHT);
    CHECK_FALSE(stick.update(0.35F, 0.0F));
}

TEST_CASE("The left stick follows the larger axis and holds it against a small wobble", "[gamepad][stick]")
{
    DirectionSelector stick(STICK_SELECTOR);
    CHECK(stick.update(-0.7F, 0.65F) == Direction::LEFT);
    CHECK(stick.update(-0.7F, 0.8F) == Direction::LEFT);
    CHECK(stick.update(-0.7F, 0.85F) == Direction::UP);
}
