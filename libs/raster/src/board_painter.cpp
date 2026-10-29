// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/raster/board_painter.hpp"

#include "snake/raster/palette.hpp"
#include "snake/sim/cell.hpp"
#include "snake/sim/direction.hpp"
#include "snake/sim/simulation.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace snake::raster
{

namespace
{

/// Measures of the design, in 32nds of a cell. The Qt scene (apps/snake/src/board/board_scene.cpp) draws the same
/// pieces with the same numbers.
constexpr int UNITS = 32;
constexpr int SEGMENT_RADIUS = 7;
constexpr int HEAD_RADIUS = 10;
constexpr int BEVEL = 5;
constexpr int FOOD_INSET = 4;
constexpr int FOOD_SIZE = 24;

/// A rectangle of pixels: `left` and `top` are in it, `right` and `bottom` are the first pixels past it.
struct Rect
{
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;

    [[nodiscard]] constexpr int width() const noexcept
    {
        return right - left;
    }
    [[nodiscard]] constexpr int height() const noexcept
    {
        return bottom - top;
    }
};

/// `value` divided by `divisor`, rounded to the nearest whole number with halves going up, for a divisor above zero.
constexpr int rounded_div(int value, int divisor) noexcept
{
    const int half = divisor / 2;
    const int shifted = value + half;
    // Integer division rounds towards zero; a negative numerator needs the floor.
    return shifted >= 0 ? shifted / divisor : -((-shifted + divisor - 1) / divisor);
}

/// A length of `units` 32nds of a cell, in pixels.
constexpr int scaled(int units, int cell) noexcept
{
    return rounded_div(units * cell, UNITS);
}

/// A rectangle given in 32nds of a cell, with its top left at (`x`, `y`) pixels, to pixels. Both edges are rounded, so
/// neighbouring rectangles share an edge, and a rectangle that is not empty is at least a pixel in each direction.
constexpr Rect to_pixels(int x, int y, int cell, int ux, int uy, int uw, int uh) noexcept
{
    const int left = scaled(ux, cell);
    const int top = scaled(uy, cell);
    return {.left = x + left,
            .top = y + top,
            .right = x + std::max(scaled(ux + uw, cell), left + 1),
            .bottom = y + std::max(scaled(uy + uh, cell), top + 1)};
}

/// The colour that `over` makes of `base` at `percent` percent, as 0xRRGGBB.
constexpr std::uint32_t blend(std::uint32_t base, std::uint32_t over, int percent) noexcept
{
    std::uint32_t result = 0;
    for (const unsigned shift : {16U, 8U, 0U})
    {
        const auto from = static_cast<int>((base >> shift) & 0xFFU);
        const auto to = static_cast<int>((over >> shift) & 0xFFU);
        result |= static_cast<std::uint32_t>(((from * (100 - percent)) + (to * percent) + 50) / 100) << shift;
    }
    return result;
}

/// The whole number whose square is nearest below or at `value`.
constexpr int integer_sqrt(int value) noexcept
{
    int root = 0;
    while ((root + 1) * (root + 1) <= value)
    {
        ++root;
    }
    return root;
}

class Painter
{
public:
    Painter(const Surface& surface, int cell, int x, int y) noexcept : m_Surface(surface), m_Cell(cell), m_X(x), m_Y(y)
    {
    }

    /// Fills `rect`, which may reach outside the surface, with `rgb`.
    void fill(Rect rect, std::uint32_t rgb) const noexcept
    {
        const int left = std::max(rect.left, 0);
        const int top = std::max(rect.top, 0);
        const int right = std::min(rect.right, m_Surface.width);
        const int bottom = std::min(rect.bottom, m_Surface.height);
        if (right <= left || bottom <= top)
        {
            return;
        }
        const std::uint16_t pixel = rgb565(rgb);
        for (int row = top; row < bottom; ++row)
        {
            std::uint16_t* line = m_Surface.pixels + (static_cast<std::ptrdiff_t>(row) * m_Surface.width);
            std::fill(line + left, line + right, pixel);
        }
    }

    /// A rectangle in 32nds of a cell from the top left of cell (`column`, `row`).
    [[nodiscard]] Rect in_cell(sim::Cell at, int ux, int uy, int uw, int uh) const noexcept
    {
        return to_pixels(m_X + (at.x * m_Cell), m_Y + (at.y * m_Cell), m_Cell, ux, uy, uw, uh);
    }

    /// A rectangle with rounded corners as three bands: the middle at full width and the top and bottom with the
    /// corners cut, and the bottom `bevel` pixels in `bevel_rgb`.
    void rounded(Rect rect, int radius, std::uint32_t rgb, int bevel = 0, std::uint32_t bevel_rgb = 0) const noexcept
    {
        radius = std::min({radius, rect.width() / 2, rect.height() / 2});
        const int band = radius / 2;
        const int cut = (radius * 34 + 50) / 100;
        const int bevel_top = bevel > 0 ? rect.bottom - bevel : rect.bottom;
        const auto add = [&](int left, int top, int right, int bottom)
        {
            if (right <= left || bottom <= top)
            {
                return;
            }
            const int split = std::clamp(bevel_top, top, bottom);
            fill({left, top, right, split}, rgb);
            fill({left, split, right, bottom}, bevel_rgb);
        };
        add(rect.left + cut, rect.top, rect.right - cut, rect.top + band);
        add(rect.left, rect.top + band, rect.right, rect.bottom - band);
        add(rect.left + cut, rect.bottom - band, rect.right - cut, rect.bottom);
    }

    /// A disc in rows of `step` pixels. The part of each row that a second disc, moved by (-shift, -shift), does not
    /// cover is drawn in `shade_rgb`: the crescent on the lower right that the design gives the food.
    void disc(Rect box, int step, int shift, std::uint32_t rgb, std::uint32_t shade_rgb) const noexcept
    {
        // Twice the coordinates, so that the centre of an even box is a whole number.
        const int radius2 = box.width();
        const int centre_x2 = box.left + box.right;
        const int centre_y2 = box.top + box.bottom;
        for (int top = box.top; top < box.bottom; top += step)
        {
            const int bottom = std::min(top + step, box.bottom);
            const int mid2 = top + bottom - centre_y2;
            const int half2 = half_chord2(radius2, mid2);
            const int left = (centre_x2 - half2 + 1) / 2;
            const int right = (centre_x2 + half2) / 2;
            if (right <= left)
            {
                continue;
            }
            const int lit_half2 = half_chord2(radius2, mid2 + (2 * shift));
            const int lit_left = std::max(left, (centre_x2 - (2 * shift) - lit_half2 + 1) / 2);
            const int lit_right = std::min(right, (centre_x2 - (2 * shift) + lit_half2) / 2);
            if (lit_right <= lit_left)
            {
                fill({left, top, right, bottom}, shade_rgb);
                continue;
            }
            fill({lit_left, top, lit_right, bottom}, rgb);
            fill({left, top, lit_left, bottom}, shade_rgb);
            fill({lit_right, top, right, bottom}, shade_rgb);
        }
    }

private:
    /// Twice the half width of a disc of twice-radius `radius2` at twice-offset `offset2` from its centre, or 0 beyond
    /// it.
    [[nodiscard]] static int half_chord2(int radius2, int offset2) noexcept
    {
        const int squared = (radius2 * radius2) - (offset2 * offset2);
        return squared > 0 ? integer_sqrt(squared) : 0;
    }

    Surface m_Surface;
    int m_Cell;
    int m_X;
    int m_Y;
};

std::uint32_t stripe_rgb(std::size_t index) noexcept
{
    return index % 2 == 0 ? palette::ACCENT : palette::SURFACE;
}

void add_food(const Painter& painter, int cell, sim::Cell at, int lift, std::uint32_t rgb, bool leaf) noexcept
{
    // The food sits at the top left of its cell, `lift` pixels above where it rests.
    const auto box = [&](int ux, int uy, int uw, int uh)
    {
        Rect rect = painter.in_cell(at, ux, uy, uw, uh);
        rect.top -= lift;
        rect.bottom -= lift;
        return rect;
    };
    const int pixel = std::max(1, cell / 16);
    painter.disc(box(FOOD_INSET, FOOD_INSET, FOOD_SIZE, FOOD_SIZE), pixel, scaled(3, cell), rgb,
                 blend(rgb, palette::INK, 35));
    painter.fill(box(FOOD_INSET + 5, FOOD_INSET + 5, 6, 6), palette::FOOD_HIGHLIGHT);
    if (leaf)
    {
        painter.rounded(box(FOOD_INSET + 8, FOOD_INSET - 6, 10, 6), 2, palette::LEAF);
    }
}

void add_portal(const Painter& painter, int cell, sim::Cell at, std::uint32_t rgb, bool dot) noexcept
{
    const int pixel = std::max(1, cell / 16);
    painter.disc(painter.in_cell(at, 2, 2, 28, 28), pixel, 0, rgb, rgb);
    painter.disc(painter.in_cell(at, 10, 10, 12, 12), pixel, 0, palette::INK, palette::INK);
    if (dot)
    {
        painter.fill(painter.in_cell(at, 14, 14, 4, 4), rgb);
    }
}

void add_bomb_food(const Painter& painter, int cell, sim::Cell at, int lift, bool spark_pink) noexcept
{
    const auto box = [&](int ux, int uy, int uw, int uh)
    {
        Rect rect = painter.in_cell(at, ux, uy, uw, uh);
        rect.top -= lift;
        rect.bottom -= lift;
        return rect;
    };
    const int pixel = std::max(1, cell / 16);
    painter.disc(box(FOOD_INSET - 1, FOOD_INSET + 1, FOOD_SIZE + 2, FOOD_SIZE + 2), pixel, 0, palette::SURFACE,
                 palette::SURFACE);
    painter.disc(box(FOOD_INSET + 1, FOOD_INSET + 3, FOOD_SIZE - 2, FOOD_SIZE - 2), pixel, scaled(3, cell),
                 palette::INK, palette::SHADOW);
    painter.fill(box(FOOD_INSET + 6, FOOD_INSET + 8, 5, 5), palette::FOOD_HIGHLIGHT);
    painter.fill(box(19, 2, 3, 8), palette::SURFACE);
    painter.fill(box(20, -3, 7, 5), spark_pink ? palette::TONGUE : palette::MEDAL_GOLD);
}

void add_bomb_in_body(const Painter& painter, int cell, sim::Cell at, bool spark_pink) noexcept
{
    const int pixel = std::max(1, cell / 16);
    painter.disc(painter.in_cell(at, 7, 8, 18, 18), pixel, 0, palette::SURFACE, palette::SURFACE);
    painter.disc(painter.in_cell(at, 9, 10, 14, 14), pixel, 0, palette::INK, palette::INK);
    painter.fill(painter.in_cell(at, 14, 2, 4, 5), palette::SURFACE);
    painter.fill(painter.in_cell(at, 12, 0, 8, 3), spark_pink ? palette::TONGUE : palette::MEDAL_GOLD);
}

void add_segment(const Painter& painter, int cell, sim::Cell at, std::uint32_t rgb, int radius_units) noexcept
{
    const Rect rect = painter.in_cell(at, 0, 0, UNITS, UNITS);
    painter.rounded(rect, scaled(radius_units, cell), rgb, scaled(BEVEL, cell), blend(rgb, palette::INK, 28));
}

/// A rectangle given as if the snake faced right, turned to face `heading`, inside the cell `at`.
Rect turned(const Painter& painter, sim::Cell at, sim::Direction heading, int x, int y, int w, int h) noexcept
{
    switch (heading)
    {
    case sim::Direction::DOWN:
        return painter.in_cell(at, UNITS - y - h, x, h, w);
    case sim::Direction::LEFT:
        return painter.in_cell(at, UNITS - x - w, UNITS - y - h, w, h);
    case sim::Direction::UP:
        return painter.in_cell(at, y, UNITS - x - w, h, w);
    case sim::Direction::RIGHT:
        break;
    }
    return painter.in_cell(at, x, y, w, h);
}

void add_head(const Painter& painter, int cell, sim::Cell at, sim::Direction heading, bool dizzy) noexcept
{
    add_segment(painter, cell, at, palette::ACCENT, HEAD_RADIUS);
    for (const int y : {4, 18})
    {
        painter.fill(turned(painter, at, heading, 16, y, 10, 10), palette::EYE);
        if (dizzy)
        {
            // A cross in place of the pupil.
            for (const auto& [dx, dy] :
                 {std::pair{4, 4}, std::pair{1, 1}, std::pair{7, 1}, std::pair{1, 7}, std::pair{7, 7}})
            {
                painter.fill(turned(painter, at, heading, 16 + dx, y + dy, 2, 2), palette::INK);
            }
        }
        else
        {
            painter.fill(turned(painter, at, heading, 21, y + 2, 5, 5), palette::INK);
        }
    }
    // The tongue is left out: it reaches into the next cell, which may be the frame.
}

} // namespace

void paint_board(const Surface& surface, int cell, const sim::Simulation& game, Look look) noexcept
{
    if (surface.pixels == nullptr || cell < 8 || surface.width < board_side(cell) || surface.height < board_side(cell))
    {
        return;
    }
    const int frame = frame_of(cell);
    const Painter painter(surface, cell, frame, frame);

    // The frame, with its corners cut, and the board inside it.
    const int side = board_side(cell);
    painter.rounded({0, 0, side, side}, 2 * frame, palette::INK);
    painter.fill({frame, frame, frame + (sim::BOARD_COLUMNS * cell), frame + (sim::BOARD_ROWS * cell)},
                 palette::BOARD_DARK);
    for (std::uint8_t y = 0; y < sim::BOARD_ROWS; ++y)
    {
        for (std::uint8_t x = 0; x < sim::BOARD_COLUMNS; ++x)
        {
            if ((x + y) % 2 == 1)
            {
                painter.fill(painter.in_cell({x, y}, 0, 0, UNITS, UNITS), palette::BOARD_LIGHT);
            }
        }
    }

    if (game.portals())
    {
        const auto& portals = game.portals()->cells;
        for (std::size_t index = 0; index < portals.size(); ++index)
        {
            const bool first_pair = index < 2;
            add_portal(painter, cell, portals[index], first_pair ? palette::TONGUE : palette::MEDAL_SILVER, first_pair);
        }
    }
    if (!game.won())
    {
        add_food(painter, cell, game.food(), look.lift, palette::FOOD, true);
    }
    if (game.bonus())
    {
        add_food(painter, cell, game.bonus()->cell, look.lift, palette::MEDAL_GOLD, false);
    }
    if (game.potato())
    {
        const std::uint8_t left = game.potato()->ticks_left;
        if (left > 10 || (left / 2) % 2 == 0)
        {
            add_bomb_food(painter, cell, game.potato()->cell, look.lift, look.spark);
        }
    }

    // The snake from the tail to the head, so the head is on top. The bomb in the body flashes pink near the tail.
    const auto& snake = game.snake();
    const std::optional<std::size_t> bomb = game.lump_index();
    const bool urgent = bomb && *bomb + 3 >= snake.size() && game.running();
    const bool flash_on = urgent && (game.ticks() % 2 == 0);
    for (std::size_t index = snake.size(); index > 1; --index)
    {
        std::uint32_t rgb = stripe_rgb(index - 1);
        if (flash_on && bomb && *bomb == index - 1)
        {
            rgb = palette::TONGUE;
        }
        add_segment(painter, cell, snake[index - 1], rgb, SEGMENT_RADIUS);
    }
    add_head(painter, cell, game.head(), game.heading(), !game.running() && !game.won());
    if (bomb)
    {
        add_bomb_in_body(painter, cell, snake[*bomb], flash_on || look.spark);
    }
}

} // namespace snake::raster
