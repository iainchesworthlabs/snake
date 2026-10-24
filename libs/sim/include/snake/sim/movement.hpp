// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/sim/cell.hpp"
#include "snake/sim/direction.hpp"
#include "snake/sim/edge_rule.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace snake::sim
{

/// The cell one step from `from` in `direction`. With the WRAP rule a step off one edge lands on the opposite edge.
/// With WALLS it returns nothing when the step leaves the board.
[[nodiscard]] constexpr std::optional<Cell> step(Cell from, Direction direction, EdgeRule edges) noexcept
{
    // Indexed by Direction: up, down, left, right.
    constexpr int DX[] = {0, 0, -1, 1};
    constexpr int DY[] = {-1, 1, 0, 0};
    const auto index = static_cast<std::size_t>(direction) % 4;
    int x = from.x + DX[index];
    int y = from.y + DY[index];
    if (edges == EdgeRule::WRAP)
    {
        x = (x + BOARD_COLUMNS) % BOARD_COLUMNS;
        y = (y + BOARD_ROWS) % BOARD_ROWS;
    }
    else if (x < 0 || x >= BOARD_COLUMNS || y < 0 || y >= BOARD_ROWS)
    {
        return std::nullopt;
    }
    return Cell{static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y)};
}

} // namespace snake::sim
