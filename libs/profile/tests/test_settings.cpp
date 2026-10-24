// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/profile/settings.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace snake::profile;

TEST_CASE("The settings of a first run", "[settings]")
{
    const Settings settings;
    CHECK(settings.controls == ControlScheme::TILT);
    CHECK_FALSE(settings.invert_axes);
    CHECK(settings.sound);
    CHECK(settings.music);
    CHECK_FALSE(settings.reduced_motion);
}

TEST_CASE("The control schemes are numbered as they are saved", "[settings][format]")
{
    CHECK(static_cast<int>(ControlScheme::TILT) == 0);
    CHECK(static_cast<int>(ControlScheme::SWIPE) == 1);
    CHECK(static_cast<int>(ControlScheme::DPAD) == 2);
    CHECK(CONTROL_SCHEME_COUNT == 3);
}

TEST_CASE("Settings are equal when every setting is", "[settings]")
{
    const Settings base;
    CHECK(base == Settings{});

    Settings changed = base;
    changed.controls = ControlScheme::SWIPE;
    CHECK_FALSE(base == changed);

    changed = base;
    changed.invert_axes = true;
    CHECK_FALSE(base == changed);

    changed = base;
    changed.sound = false;
    CHECK_FALSE(base == changed);

    changed = base;
    changed.music = false;
    CHECK_FALSE(base == changed);

    changed = base;
    changed.reduced_motion = true;
    CHECK_FALSE(base == changed);
}
