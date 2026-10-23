// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

namespace snake::sim
{

/// The four directions the snake can travel. Up is towards row 0.
enum class Direction : std::uint8_t
{
    UP,
    DOWN,
    LEFT,
    RIGHT,
};

/// The direction that points the other way.
[[nodiscard]] constexpr Direction opposite(Direction direction) noexcept
{
    switch (direction)
    {
    case Direction::UP:
        return Direction::DOWN;
    case Direction::DOWN:
        return Direction::UP;
    case Direction::LEFT:
        return Direction::RIGHT;
    case Direction::RIGHT:
        return Direction::LEFT;
    }
    return direction;
}

/// True when the two directions are on the same axis and point opposite ways. A snake cannot reverse into itself.
[[nodiscard]] constexpr bool is_reversal(Direction from, Direction to) noexcept
{
    return opposite(from) == to;
}

} // namespace snake::sim
