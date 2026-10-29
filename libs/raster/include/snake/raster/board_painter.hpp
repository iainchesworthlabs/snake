// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/sim/simulation.hpp"

#include <cstdint>

namespace snake::raster
{

/// A rectangle of 16-bit pixels, in rows of `width`, that the painter draws into. It is the caller's memory: the
/// painter allocates nothing.
struct Surface
{
    std::uint16_t* pixels = nullptr;
    int width = 0;
    int height = 0;
};

/// The width of the frame round the cells, for cells `cell` pixels square.
[[nodiscard]] constexpr int frame_of(int cell) noexcept
{
    return cell / 4 > 2 ? cell / 4 : 2;
}

/// The side of the square that paint_board() fills: the cells and the frame round them.
[[nodiscard]] constexpr int board_side(int cell) noexcept
{
    return (sim::BOARD_COLUMNS * cell) + (2 * frame_of(cell));
}

/// What the board looks like besides the game: how far the food and the bonus items are raised (0 at rest), in pixels,
/// and whether the bomb food shows its spark in pink (it alternates with gold, so it flickers).
struct Look
{
    int lift = 0;
    bool spark = false;
};

/// Draws the board of `game` into `surface`, with its top left at the surface's: the frame, the checkerboard, the
/// portals, the food, the bonus item and the bomb food, the snake and the bomb in its body, in flat fills on whole
/// pixels. Cells are `cell` pixels square (at least 8 for the art to hold together). It does nothing when the surface
/// is smaller than board_side(cell) in either direction, and it never writes outside the surface.
void paint_board(const Surface& surface, int cell, const sim::Simulation& game, Look look = {}) noexcept;

} // namespace snake::raster
