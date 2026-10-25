// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

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
/// bonus item are raised, from 0 (resting) to 1 (the top of the bob).
[[nodiscard]] std::vector<Shape> game_shapes(const Layout& layout, const sim::Simulation& game, double lift);

} // namespace snake::ui::board_scene
