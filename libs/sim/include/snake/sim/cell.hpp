// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

namespace snake::sim
{

/// The board is a fixed grid, the same on every platform. The origin (0, 0) is the top left cell.
inline constexpr std::uint8_t BOARD_COLUMNS = 20;
inline constexpr std::uint8_t BOARD_ROWS = 20;
inline constexpr std::uint16_t BOARD_CELLS = BOARD_COLUMNS * BOARD_ROWS;

/// One cell of the board.
struct Cell
{
    std::uint8_t x = 0;
    std::uint8_t y = 0;

    friend constexpr bool operator==(Cell lhs, Cell rhs) noexcept = default;
};

/// The row-major index of a cell, from 0 to BOARD_CELLS - 1. The cell must be on the board.
[[nodiscard]] constexpr std::uint16_t to_index(Cell cell) noexcept
{
    return static_cast<std::uint16_t>((cell.y * BOARD_COLUMNS) + cell.x);
}

/// The cell at a row-major index. The index must be below BOARD_CELLS.
[[nodiscard]] constexpr Cell from_index(std::uint16_t index) noexcept
{
    return Cell{static_cast<std::uint8_t>(index % BOARD_COLUMNS), static_cast<std::uint8_t>(index / BOARD_COLUMNS)};
}

[[nodiscard]] constexpr bool on_board(Cell cell) noexcept
{
    return cell.x < BOARD_COLUMNS && cell.y < BOARD_ROWS;
}

} // namespace snake::sim
