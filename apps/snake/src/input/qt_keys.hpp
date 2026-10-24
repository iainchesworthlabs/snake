// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/input/key_map.hpp"

#include <optional>

namespace snake::ui
{

/// The library's key for a Qt key code (a value of Qt::Key), or nothing when the game does not use the key. Both Enter
/// keys give ENTER.
[[nodiscard]] std::optional<input::Key> key_from_qt(int qt_key) noexcept;

} // namespace snake::ui
