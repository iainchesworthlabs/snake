// SPDX-License-Identifier: GPL-3.0-or-later
#include "board/board_scene.hpp"

#include "snake/sim/cell.hpp"
#include "snake/sim/direction.hpp"
#include "theme/tokens.hpp"

#include <QPoint>
#include <QRect>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace snake::ui::board_scene
{

namespace
{

/// Measures of the design, in 32nds of a cell (the desktop size). tokens.json and the desktop screen give them.
constexpr int UNITS = 32;
constexpr int SEGMENT_RADIUS = 7;
constexpr int HEAD_RADIUS = 10;
constexpr int BEVEL = 5;
constexpr int FOOD_INSET = 4;
constexpr int FOOD_SIZE = 24;
constexpr int BOB = 5;

constexpr double BEVEL_ALPHA = 0.28;
constexpr double FOOD_SHADE_ALPHA = 0.35;

constexpr QRgb opaque(std::uint32_t rgb)
{
    return 0xFF000000U | rgb;
}

/// `over` laid on `base` at `alpha`, as the design's translucent ink shades are.
QRgb blend(QRgb base, QRgb over, double alpha)
{
    const auto channel = [&](int shift)
    {
        const double from = static_cast<double>((base >> shift) & 0xFFU);
        const double to = static_cast<double>((over >> shift) & 0xFFU);
        return static_cast<std::uint32_t>(std::lround((from * (1.0 - alpha)) + (to * alpha)));
    };
    return 0xFF000000U | (channel(16) << 16) | (channel(8) << 8) | channel(0);
}

/// A length of `units` 32nds of a cell, in pixels.
int scaled(int units, int cell)
{
    return static_cast<int>(std::lround(static_cast<double>(units * cell) / UNITS));
}

/// A rectangle given in 32nds of a cell, with its top left at `at` pixels, to pixels. Both edges are rounded, so
/// neighbouring rectangles share an edge, and a rectangle that is not empty is at least a pixel in each direction.
QRect to_pixels(QPoint at, int cell, int x, int y, int w, int h)
{
    const int left = scaled(x, cell);
    const int top = scaled(y, cell);
    const int right = std::max(scaled(x + w, cell), left + 1);
    const int bottom = std::max(scaled(y + h, cell), top + 1);
    return QRect(at.x() + left, at.y() + top, right - left, bottom - top);
}

/// A rectangle drawn for the head, given as if the snake faced right, turned to face `heading`.
QRect turned(QPoint at, int cell, sim::Direction heading, int x, int y, int w, int h)
{
    switch (heading)
    {
    case sim::Direction::DOWN:
        return to_pixels(at, cell, UNITS - y - h, x, h, w);
    case sim::Direction::LEFT:
        return to_pixels(at, cell, UNITS - x - w, UNITS - y - h, w, h);
    case sim::Direction::UP:
        return to_pixels(at, cell, y, UNITS - x - w, h, w);
    case sim::Direction::RIGHT:
        break;
    }
    return to_pixels(at, cell, x, y, w, h);
}

/// Adds a rectangle with rounded corners of `radius` pixels as three bands: the middle at full width and the top and
/// bottom with the corners cut. The cut is the width a circle of that radius leaves after half the radius. Pixels at or
/// below `bevel_top` are drawn in `bevel`, to give the lower edge its shade.
void add_rounded(std::vector<Shape>& out, QRect rect, int radius, QRgb colour, int bevel_top = -1, QRgb bevel = 0)
{
    radius = std::min({radius, rect.width() / 2, rect.height() / 2});
    const int band = radius / 2;
    const int cut = static_cast<int>(std::lround(static_cast<double>(radius) * 0.34));
    const auto add = [&](int left, int top, int right, int bottom)
    {
        if (right <= left || bottom <= top)
        {
            return;
        }
        // The part of the band above and the part below the bevel line.
        const int split = std::clamp(bevel_top < 0 ? bottom : bevel_top, top, bottom);
        if (split > top)
        {
            out.push_back({QRect(left, top, right - left, split - top), colour});
        }
        if (bottom > split)
        {
            out.push_back({QRect(left, split, right - left, bottom - split), bevel});
        }
    };
    add(rect.left() + cut, rect.top(), rect.left() + rect.width() - cut, rect.top() + band);
    add(rect.left(), rect.top() + band, rect.left() + rect.width(), rect.top() + rect.height() - band);
    add(rect.left() + cut, rect.top() + rect.height() - band, rect.left() + rect.width() - cut,
        rect.top() + rect.height());
}

/// The half width of a disc of `radius` at `offset` from its centre.
double half_chord(double radius, double offset)
{
    const double squared = (radius * radius) - (offset * offset);
    return squared > 0.0 ? std::sqrt(squared) : 0.0;
}

/// Adds a disc in rows of `step` pixels. The part of each row that a second disc, moved by (-shift, -shift), does not
/// cover is drawn in `shade`: the crescent on the lower right that the design gives the food.
void add_disc(std::vector<Shape>& out, QRect box, int step, int shift, QRgb colour, QRgb shade)
{
    const double radius = static_cast<double>(box.width()) / 2.0;
    const double centre_x = box.left() + radius;
    const double centre_y = box.top() + radius;
    for (int top = box.top(); top < box.top() + box.height(); top += step)
    {
        const int bottom = std::min(top + step, box.top() + box.height());
        const double mid = (static_cast<double>(top + bottom) / 2.0) - centre_y;
        const double half = half_chord(radius, mid);
        const int left = static_cast<int>(std::lround(centre_x - half));
        const int right = static_cast<int>(std::lround(centre_x + half));
        if (right <= left)
        {
            continue;
        }
        // The lit part: the same disc moved up and to the left, which leaves a crescent of shade on the other side.
        const double lit_half = half_chord(radius, mid + shift);
        const int lit_left = std::max(left, static_cast<int>(std::lround(centre_x - shift - lit_half)));
        const int lit_right = std::min(right, static_cast<int>(std::lround(centre_x - shift + lit_half)));
        if (lit_right <= lit_left)
        {
            out.push_back({QRect(left, top, right - left, bottom - top), shade});
            continue;
        }
        out.push_back({QRect(lit_left, top, lit_right - lit_left, bottom - top), colour});
        if (lit_left > left)
        {
            out.push_back({QRect(left, top, lit_left - left, bottom - top), shade});
        }
        if (right > lit_right)
        {
            out.push_back({QRect(lit_right, top, right - lit_right, bottom - top), shade});
        }
    }
}

QPoint cell_corner(const Layout& layout, sim::Cell at)
{
    return QPoint(layout.origin.x() + (at.x * layout.cell), layout.origin.y() + (at.y * layout.cell));
}

/// A blueberry-like piece of food at the top left of its cell: a disc, a highlight and a leaf. `rise` is in pixels.
void add_food(std::vector<Shape>& out, const Layout& layout, QPoint at, int rise, std::uint32_t body_rgb)
{
    const int cell = layout.cell;
    const QPoint top_left(at.x(), at.y() - rise);
    const QRect box = to_pixels(top_left, cell, FOOD_INSET, FOOD_INSET, FOOD_SIZE, FOOD_SIZE);
    const QRgb body = opaque(body_rgb);
    add_disc(out, box, layout.pixel(), scaled(3, cell), body, blend(body, opaque(tokens::INK), FOOD_SHADE_ALPHA));
    out.push_back({to_pixels(top_left, cell, FOOD_INSET + 5, FOOD_INSET + 5, 6, 6), opaque(tokens::FOOD_HIGHLIGHT)});
}

void add_leaf(std::vector<Shape>& out, const Layout& layout, QPoint at, int rise)
{
    const QPoint top_left(at.x(), at.y() - rise);
    const QRect leaf = to_pixels(top_left, layout.cell, FOOD_INSET + 8, FOOD_INSET - 6, 10, 6);
    add_rounded(out, leaf, leaf.height() / 2, opaque(tokens::LEAF));
}

/// The stripes alternate along the snake from the head, and a segment keeps its place in the snake as it moves.
QRgb stripe_colour(std::size_t index)
{
    return opaque(index % 2 == 0 ? tokens::ACCENT : tokens::SURFACE);
}

void add_segment(std::vector<Shape>& out, const Layout& layout, QPoint at, QRgb colour, int radius_units)
{
    const int cell = layout.cell;
    const QRect rect = to_pixels(at, cell, 0, 0, UNITS, UNITS);
    const int bevel = scaled(BEVEL, cell);
    add_rounded(out, rect, scaled(radius_units, cell), colour, rect.bottom() + 1 - bevel,
                blend(colour, opaque(tokens::INK), BEVEL_ALPHA));
}

void add_head(std::vector<Shape>& out, const Layout& layout, QPoint at, sim::Direction heading, const QRect& cells)
{
    const int cell = layout.cell;
    add_segment(out, layout, at, opaque(tokens::ACCENT), HEAD_RADIUS);
    // The eyes sit on the front half of the head, one each side, with the pupils looking ahead.
    for (const int y : {4, 18})
    {
        out.push_back({turned(at, cell, heading, 16, y, 10, 10), opaque(tokens::EYE)});
        out.push_back({turned(at, cell, heading, 21, y + 2, 5, 5), opaque(tokens::INK)});
    }
    // The tongue reaches into the next cell and stops at the edge of the board.
    const QRect tongue = turned(at, cell, heading, UNITS, 14, 10, 3).intersected(cells);
    if (!tongue.isEmpty())
    {
        out.push_back({tongue, opaque(tokens::TONGUE)});
    }
}

/// What the snake does on the next tick, as far as the picture needs to know.
struct Motion
{
    sim::Direction facing = sim::Direction::RIGHT; ///< Where the head looks.
    bool moving = false;                           ///< The head moves into `next` without the game ending.
    bool eating = false;                           ///< It eats there, so the tail stays.
    sim::Cell next;
};

Motion motion_of(const sim::Simulation& game)
{
    const auto& turns = game.pending_turns();
    Motion motion;
    motion.facing = turns.empty() ? game.heading() : turns.at(0);
    if (!game.running() || game.won())
    {
        return motion;
    }
    const sim::Cell head = game.head();
    const int x = head.x + (motion.facing == sim::Direction::RIGHT  ? 1
                            : motion.facing == sim::Direction::LEFT ? -1
                                                                    : 0);
    const int y = head.y + (motion.facing == sim::Direction::DOWN ? 1 : motion.facing == sim::Direction::UP ? -1 : 0);
    // Off the board the game ends, or the snake comes out of the other side, which is not drawn as a glide.
    if (x < 0 || y < 0 || x >= sim::BOARD_COLUMNS || y >= sim::BOARD_ROWS)
    {
        return motion;
    }
    motion.next = sim::Cell{static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y)};
    const bool bonus = game.bonus() && game.bonus()->cell == motion.next;
    motion.eating = motion.next == game.food() || bonus;

    // The body stops the snake, except for the tail when it moves away.
    const auto& snake = game.snake();
    const std::size_t blocking = motion.eating ? snake.size() : snake.size() - 1;
    for (std::size_t index = 0; index < blocking; ++index)
    {
        if (snake[index] == motion.next)
        {
            return motion;
        }
    }
    motion.moving = true;
    return motion;
}

/// `progress` of the way from one pixel position to another, on whole pixels.
QPoint glide(QPoint from, QPoint to, double progress)
{
    return QPoint(from.x() + static_cast<int>(std::lround(static_cast<double>(to.x() - from.x()) * progress)),
                  from.y() + static_cast<int>(std::lround(static_cast<double>(to.y() - from.y()) * progress)));
}

} // namespace

QRect Layout::cells() const
{
    return QRect(origin.x(), origin.y(), cell * sim::BOARD_COLUMNS, cell * sim::BOARD_ROWS);
}

QRect Layout::outer() const
{
    return cells().adjusted(-frame, -frame, frame, frame);
}

int Layout::pixel() const
{
    return std::max(1, cell / 16);
}

Layout layout_for(QSize available)
{
    const auto frame_for = [](int cell) { return std::max(2, static_cast<int>(std::lround(cell / 4.0))); };
    const int columns = sim::BOARD_COLUMNS;
    const int rows = sim::BOARD_ROWS;
    int cell = std::min(available.width() / columns, available.height() / rows);
    while (cell > 0)
    {
        const int frame = frame_for(cell);
        if ((columns * cell) + (2 * frame) <= available.width() &&
            (rows * cell) + (2 * frame) + (2 * frame) <= available.height())
        {
            break;
        }
        --cell;
    }
    if (cell <= 0)
    {
        return Layout{};
    }
    Layout layout;
    layout.cell = cell;
    layout.frame = frame_for(cell);
    layout.drop = 2 * layout.frame;
    const int width = (columns * cell) + (2 * layout.frame);
    const int height = (rows * cell) + (2 * layout.frame) + layout.drop;
    layout.origin =
        QPoint(((available.width() - width) / 2) + layout.frame, ((available.height() - height) / 2) + layout.frame);
    return layout;
}

std::vector<Shape> background_shapes(const Layout& layout)
{
    std::vector<Shape> shapes;
    if (layout.cell <= 0)
    {
        return shapes;
    }
    const QRect outer = layout.outer();
    const int radius = 2 * layout.frame;
    add_rounded(shapes, outer.translated(0, layout.drop), radius, opaque(tokens::SHADOW));
    add_rounded(shapes, outer, radius, opaque(tokens::INK));

    const QRect cells = layout.cells();
    shapes.push_back({cells, opaque(tokens::BOARD_DARK)});
    for (int y = 0; y < sim::BOARD_ROWS; ++y)
    {
        for (int x = 0; x < sim::BOARD_COLUMNS; ++x)
        {
            if ((x + y) % 2 == 1)
            {
                shapes.push_back(
                    {QRect(cells.left() + (x * layout.cell), cells.top() + (y * layout.cell), layout.cell, layout.cell),
                     opaque(tokens::BOARD_LIGHT)});
            }
        }
    }
    return shapes;
}

std::vector<Shape> game_shapes(const Layout& layout, const sim::Simulation& game, double lift, double progress)
{
    std::vector<Shape> shapes;
    if (layout.cell <= 0)
    {
        return shapes;
    }
    const int rise = static_cast<int>(std::lround(std::clamp(lift, 0.0, 1.0) * scaled(BOB, layout.cell)));

    // After a win there is no room for the food.
    if (!game.won())
    {
        const QPoint at = cell_corner(layout, game.food());
        add_food(shapes, layout, at, rise, tokens::FOOD);
        add_leaf(shapes, layout, at, rise);
    }
    if (game.bonus())
    {
        add_food(shapes, layout, cell_corner(layout, game.bonus()->cell), rise, tokens::MEDAL_GOLD);
    }

    const Motion motion = motion_of(game);
    const double along = motion.moving ? std::clamp(progress, 0.0, 1.0) : 0.0;
    const auto& snake = game.snake();
    // A piece of the snake moves into the cell of the piece in front of it, unless that is across the edge.
    const auto position_of = [&](std::size_t index)
    {
        const QPoint from = cell_corner(layout, snake[index]);
        if (index == 0)
        {
            return glide(from, cell_corner(layout, motion.next), along);
        }
        const sim::Cell ahead = snake[index - 1];
        const bool adjacent = std::abs(ahead.x - snake[index].x) + std::abs(ahead.y - snake[index].y) == 1;
        return adjacent ? glide(from, cell_corner(layout, ahead), along) : from;
    };

    // The tail first, so the head is on top. A snake that eats grows a piece behind its tail, which the tail uncovers
    // as it moves on.
    if (motion.moving && motion.eating && along > 0.0)
    {
        add_segment(shapes, layout, cell_corner(layout, snake.back()), stripe_colour(snake.size()), SEGMENT_RADIUS);
    }
    for (std::size_t index = snake.size(); index > 1; --index)
    {
        add_segment(shapes, layout, position_of(index - 1), stripe_colour(index - 1), SEGMENT_RADIUS);
    }
    add_head(shapes, layout, position_of(0), motion.facing, layout.cells());
    return shapes;
}

} // namespace snake::ui::board_scene
