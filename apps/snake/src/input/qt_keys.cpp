// SPDX-License-Identifier: GPL-3.0-or-later
#include "input/qt_keys.hpp"

#include "snake/input/key_map.hpp"

#include <Qt>
#include <optional>

namespace snake::ui
{

std::optional<input::Key> key_from_qt(int qt_key) noexcept
{
    switch (qt_key)
    {
    case Qt::Key_Up:
        return input::Key::ARROW_UP;
    case Qt::Key_Down:
        return input::Key::ARROW_DOWN;
    case Qt::Key_Left:
        return input::Key::ARROW_LEFT;
    case Qt::Key_Right:
        return input::Key::ARROW_RIGHT;
    case Qt::Key_W:
        return input::Key::W;
    case Qt::Key_A:
        return input::Key::A;
    case Qt::Key_S:
        return input::Key::S;
    case Qt::Key_D:
        return input::Key::D;
    case Qt::Key_Space:
        return input::Key::SPACE;
    case Qt::Key_Escape:
        return input::Key::ESCAPE;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return input::Key::ENTER;
    default:
        return std::nullopt;
    }
}

} // namespace snake::ui
