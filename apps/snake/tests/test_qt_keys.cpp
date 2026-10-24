// SPDX-License-Identifier: GPL-3.0-or-later
#include "input/qt_keys.hpp"
#include "snake/input/key_map.hpp"

#include <catch2/catch_test_macros.hpp>

#include <Qt>

using snake::input::Key;
using snake::ui::key_from_qt;

TEST_CASE("Qt keys map to the keys of the game", "[keys]")
{
    CHECK(key_from_qt(Qt::Key_Up) == Key::ARROW_UP);
    CHECK(key_from_qt(Qt::Key_Down) == Key::ARROW_DOWN);
    CHECK(key_from_qt(Qt::Key_Left) == Key::ARROW_LEFT);
    CHECK(key_from_qt(Qt::Key_Right) == Key::ARROW_RIGHT);
    CHECK(key_from_qt(Qt::Key_W) == Key::W);
    CHECK(key_from_qt(Qt::Key_A) == Key::A);
    CHECK(key_from_qt(Qt::Key_S) == Key::S);
    CHECK(key_from_qt(Qt::Key_D) == Key::D);
    CHECK(key_from_qt(Qt::Key_Space) == Key::SPACE);
    CHECK(key_from_qt(Qt::Key_Escape) == Key::ESCAPE);
}

TEST_CASE("Both Enter keys give Enter", "[keys]")
{
    CHECK(key_from_qt(Qt::Key_Return) == Key::ENTER);
    CHECK(key_from_qt(Qt::Key_Enter) == Key::ENTER);
}

TEST_CASE("Other keys are not used", "[keys]")
{
    CHECK_FALSE(key_from_qt(Qt::Key_Q));
    CHECK_FALSE(key_from_qt(Qt::Key_F1));
    CHECK_FALSE(key_from_qt(Qt::Key_unknown));
    CHECK_FALSE(key_from_qt(0));
}
