// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "board/effects.hpp"
#include "snake/sim/simulation.hpp"

#include <QRect>
#include <QRgb>
#include <QSize>
#include <vector>

namespace snake::ui::board_scene
{

/// A filled rectangle in item coordinates. All of the art is rectangles on whole pixels, which keeps the edges crisp
/// and is what every renderer, including the software one, can draw.
struct Shape
{
    QRect rect;
    QRgb colour = 0;
};

/// Where the board is in an item: the cells are `cell` pixels square, in a frame of `frame` pixels, and the frame
/// casts a hard shadow `drop` pixels down. The cell size is a whole number, so the pixels of the art stay crisp, and
/// the frame, the cells and the shadow together are centred in the item.
struct Layout
{
    int cell = 0;
    int frame = 0;
    int drop = 0;
    QPoint origin; ///< The top left of the first cell.

    /// The part of the item the cells fill.
    [[nodiscard]] QRect cells() const;
    /// The frame, around the cells.
    [[nodiscard]] QRect outer() const;

    /// The pixel unit of the art: the food's highlight, the eyes and the steps of a curve are made of these.
    [[nodiscard]] int pixel() const;

    friend bool operator==(const Layout& lhs, const Layout& rhs) noexcept = default;
};

/// The largest layout that fits `available`. The cell size is zero when nothing fits.
[[nodiscard]] Layout layout_for(QSize available);

/// The shadow, the frame and the checkerboard. They do not change while the game runs, so a layout draws them once.
[[nodiscard]] std::vector<Shape> background_shapes(const Layout& layout);

/// The snake, the food and the bonus item of `game`, in the order they are drawn. `lift` is how far the food and the
/// bonus item are raised, from 0 (resting) to 1 (the top of the bob). `progress` is how far the game is from this tick
/// to the next, from 0 to 1: the snake is drawn that far into its next move, so that it glides from cell to cell. Each
/// segment moves into the cell of the one in front of it and the head moves into the cell it will take, facing the
/// way it will turn. A move that ends the game, that crosses the edge of a wrapping board or that is not coming (the
/// game is not running) is not drawn ahead. When the snake is about to eat, its tail stays and the body stretches.
[[nodiscard]] std::vector<Shape> game_shapes(const Layout& layout, const sim::Simulation& game, double lift,
                                             double progress = 0.0);

/// The burst of specks and the rising points of `effects`, drawn over the game. A speck is a square a fifth of a cell
/// wide and a score is "+" and the digits of the points in a 3 by 5 pixel font, in cream on an ink shadow, which has to
/// read on the teal board. Things outside the cells are left out.
[[nodiscard]] std::vector<Shape> effect_shapes(const Layout& layout, const effects::Effects& effects);

/// The squares of one glyph of the pixel font ("0" to "9" and "+"), as columns and rows from the top left of a 3 by 5
/// grid. Other characters have none.
[[nodiscard]] std::vector<QPoint> glyph_pixels(char glyph);

} // namespace snake::ui::board_scene
