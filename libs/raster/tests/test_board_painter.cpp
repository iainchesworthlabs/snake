// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/raster/board_painter.hpp"
#include "snake/raster/palette.hpp"
#include "snake/sim/autoplayer.hpp"
#include "snake/sim/simulation.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

using namespace snake::raster;
using snake::sim::Cell;
using snake::sim::Simulation;
using snake::sim::Variant;

namespace
{

constexpr std::uint16_t UNTOUCHED = 0x1234;

/// A surface that is `extra` pixels larger than the board in each direction, filled with a value the painter never
/// draws.
struct Canvas
{
    explicit Canvas(int cell, int extra = 0)
        : side(board_side(cell)), width(board_side(cell) + extra), height(board_side(cell) + extra),
          pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), UNTOUCHED)
    {
    }

    [[nodiscard]] Surface surface()
    {
        return {.pixels = pixels.data(), .width = width, .height = height};
    }

    [[nodiscard]] std::uint16_t at(int x, int y) const
    {
        return pixels[(static_cast<std::size_t>(y) * static_cast<std::size_t>(width)) + static_cast<std::size_t>(x)];
    }

    int side;
    int width;
    int height;
    std::vector<std::uint16_t> pixels;
};

/// The pixel in the middle of a part of a cell, given in 32nds of the cell.
std::uint16_t probe(const Canvas& canvas, int cell, Cell at, int ux, int uy)
{
    const int frame = frame_of(cell);
    const int x = frame + (at.x * cell) + ((ux * cell) / 32);
    const int y = frame + (at.y * cell) + ((uy * cell) / 32);
    return canvas.at(x, y);
}

Simulation game_with(const std::vector<Cell>& snake, Cell food, Variant variant = Variant::PLAIN)
{
    const auto game = Simulation::from_setup({.snake = snake, .food = food, .seed = 1, .variant = variant});
    REQUIRE(game.has_value());
    return *game;
}

constexpr int CELL = 24;

} // namespace

TEST_CASE("A colour is cut down to 5, 6 and 5 bits", "[raster]")
{
    CHECK(rgb565(0x000000) == 0x0000);
    CHECK(rgb565(0xFFFFFF) == 0xFFFF);
    CHECK(rgb565(0xFF0000) == 0xF800);
    CHECK(rgb565(0x00FF00) == 0x07E0);
    CHECK(rgb565(0x0000FF) == 0x001F);
    CHECK(rgb565(0xFF6B4A) == 0xFB49);
}

TEST_CASE("The board is the cells and a frame round them, a quarter of a cell wide and at least 2 pixels", "[raster]")
{
    CHECK(frame_of(8) == 2);
    CHECK(frame_of(12) == 3);
    CHECK(frame_of(24) == 6);
    CHECK(board_side(24) == (20 * 24) + 12);
    CHECK(board_side(8) == (20 * 8) + 4);
}

TEST_CASE("The frame is ink and the cells are a checkerboard of the two teals", "[raster][board]")
{
    Canvas canvas(CELL);
    paint_board(canvas.surface(), CELL, game_with({{10, 10}, {9, 10}, {8, 10}}, {2, 2}));
    const int frame = frame_of(CELL);
    CHECK(canvas.at(frame / 2, canvas.side / 2) == rgb565(palette::INK));
    CHECK(canvas.at(canvas.side - 1 - (frame / 2), canvas.side / 2) == rgb565(palette::INK));
    // The corner of the frame is cut away, so that the board has rounded corners over whatever is behind it.
    CHECK(canvas.at(0, 0) == UNTOUCHED);
    // The first cell is the dark one, the next the light one, and the one below the first is light as well.
    CHECK(probe(canvas, CELL, {0, 0}, 4, 4) == rgb565(palette::BOARD_DARK));
    CHECK(probe(canvas, CELL, {1, 0}, 4, 4) == rgb565(palette::BOARD_LIGHT));
    CHECK(probe(canvas, CELL, {0, 1}, 4, 4) == rgb565(palette::BOARD_LIGHT));
    CHECK(probe(canvas, CELL, {1, 1}, 4, 4) == rgb565(palette::BOARD_DARK));
}

TEST_CASE("The food is a blue ball with a highlight and a green leaf, and it rises with the lift", "[raster][food]")
{
    Canvas rest(CELL);
    paint_board(rest.surface(), CELL, game_with({{10, 10}, {9, 10}, {8, 10}}, {2, 2}));
    CHECK(probe(rest, CELL, {2, 2}, 16, 16) == rgb565(palette::FOOD));
    CHECK(probe(rest, CELL, {2, 2}, 11, 11) == rgb565(palette::FOOD_HIGHLIGHT));
    CHECK(probe(rest, CELL, {2, 2}, 16, 1) == rgb565(palette::LEAF));

    // Lifted by 5 pixels the ball is higher: where it was at the bottom edge it is now the board.
    Canvas lifted(CELL);
    paint_board(lifted.surface(), CELL, game_with({{10, 10}, {9, 10}, {8, 10}}, {2, 2}), {.lift = 5});
    const int low = frame_of(CELL) + (2 * CELL) + ((27 * CELL) / 32);
    const int x = frame_of(CELL) + (2 * CELL) + (CELL / 2);
    CHECK(rest.at(x, low) != lifted.at(x, low));
}

TEST_CASE("The snake is coral and cream stripes from the head, with eyes on the head", "[raster][snake]")
{
    Canvas canvas(CELL);
    paint_board(canvas.surface(), CELL, game_with({{10, 10}, {9, 10}, {8, 10}}, {2, 2}));
    CHECK(probe(canvas, CELL, {10, 10}, 4, 16) == rgb565(palette::ACCENT));  // the head's back
    CHECK(probe(canvas, CELL, {9, 10}, 16, 12) == rgb565(palette::SURFACE)); // the second part is cream
    CHECK(probe(canvas, CELL, {8, 10}, 16, 12) == rgb565(palette::ACCENT));  // the third is coral again
    // Facing right, the eyes are on the front half: a white eye with an ink pupil on the near side of the head.
    CHECK(probe(canvas, CELL, {10, 10}, 18, 8) == rgb565(palette::EYE));
    CHECK(probe(canvas, CELL, {10, 10}, 23, 8) == rgb565(palette::INK));
}

TEST_CASE("The head faces the way the snake goes", "[raster][snake]")
{
    for (const auto heading : {snake::sim::Direction::UP, snake::sim::Direction::DOWN, snake::sim::Direction::LEFT})
    {
        // A snake heading `heading` from the middle, with its body behind it.
        const int dx = heading == snake::sim::Direction::LEFT ? 1 : 0;
        const int dy = heading == snake::sim::Direction::UP ? 1 : heading == snake::sim::Direction::DOWN ? -1 : 0;
        const std::vector<Cell> snake{
            {10, 10},
            {static_cast<std::uint8_t>(10 + dx), static_cast<std::uint8_t>(10 + dy)},
            {static_cast<std::uint8_t>(10 + (2 * dx)), static_cast<std::uint8_t>(10 + (2 * dy))}};
        const auto game = Simulation::from_setup({.heading = heading, .snake = snake, .food = {2, 2}, .seed = 1});
        REQUIRE(game.has_value());
        Canvas canvas(CELL);
        paint_board(canvas.surface(), CELL, *game);
        // Whichever way it faces, the head has two white eyes and two ink pupils, and the front is not the back.
        int white = 0;
        int ink = 0;
        const int left = frame_of(CELL) + (10 * CELL);
        const int top = frame_of(CELL) + (10 * CELL);
        for (int y = top; y < top + CELL; ++y)
        {
            for (int x = left; x < left + CELL; ++x)
            {
                white += canvas.at(x, y) == rgb565(palette::EYE) ? 1 : 0;
                ink += canvas.at(x, y) == rgb565(palette::INK) ? 1 : 0;
            }
        }
        CHECK(white > 10);
        CHECK(ink > 4);
    }
}

TEST_CASE("A snake that crashed has crosses in its eyes", "[raster][snake]")
{
    Simulation game = game_with({{19, 10}, {18, 10}, {17, 10}}, {2, 2});
    REQUIRE(game.tick() == snake::sim::TickOutcome::HIT_WALL);
    Canvas dizzy(CELL);
    paint_board(dizzy.surface(), CELL, game);
    const Simulation alive = game_with({{19, 10}, {18, 10}, {17, 10}}, {2, 2});
    Canvas well(CELL);
    paint_board(well.surface(), CELL, alive);
    CHECK(dizzy.pixels != well.pixels);
}

TEST_CASE("Portals are rings of their pair's colour round an ink hole, and the first pair has a dot",
          "[raster][portals]")
{
    const Simulation::PortalCells cells{{Cell{3, 3}, Cell{15, 4}, Cell{4, 15}, Cell{16, 16}}};
    const std::vector<Cell> snake{{10, 10}, {9, 10}, {8, 10}};
    const auto game = Simulation::from_setup(
        {.snake = snake, .food = {6, 7}, .seed = 1, .variant = Variant::PORTALS, .portals = cells});
    REQUIRE(game.has_value());
    Canvas canvas(CELL);
    paint_board(canvas.surface(), CELL, *game);
    for (std::size_t index = 0; index < cells.cells.size(); ++index)
    {
        const bool first_pair = index < 2;
        INFO("portal " << index);
        CHECK(probe(canvas, CELL, cells.cells[index], 4, 16) ==
              rgb565(first_pair ? palette::TONGUE : palette::MEDAL_SILVER));
        // The hole is ink; the dot of the first pair is in the middle of it.
        CHECK(probe(canvas, CELL, cells.cells[index], 12, 16) == rgb565(palette::INK));
        CHECK(probe(canvas, CELL, cells.cells[index], 16, 16) == rgb565(first_pair ? palette::TONGUE : palette::INK));
    }
}

TEST_CASE("The bomb food is an ink ball in a cream rim, and it blinks in its last ten ticks", "[raster][potato]")
{
    const std::vector<Cell> snake{{10, 10}, {9, 10}, {8, 10}, {7, 10}, {6, 10}, {5, 10}};
    const auto build = [&](std::uint8_t left)
    {
        const auto game = Simulation::from_setup({.snake = snake,
                                                  .food = {2, 2},
                                                  .seed = 1,
                                                  .variant = Variant::HOT_POTATO,
                                                  .potato = Simulation::Potato{.cell = {14, 5}, .ticks_left = left}});
        REQUIRE(game.has_value());
        return *game;
    };
    Canvas fresh(CELL);
    paint_board(fresh.surface(), CELL, build(40));
    CHECK(probe(fresh, CELL, {14, 5}, 16, 18) == rgb565(palette::INK));
    CHECK(probe(fresh, CELL, {14, 5}, 5, 16) == rgb565(palette::SURFACE));

    // Under ten ticks it is drawn for two ticks and left out for two.
    Canvas lit(CELL);
    paint_board(lit.surface(), CELL, build(8));
    Canvas dark(CELL);
    paint_board(dark.surface(), CELL, build(7));
    CHECK(probe(lit, CELL, {14, 5}, 16, 18) == rgb565(palette::INK));
    CHECK(probe(dark, CELL, {14, 5}, 16, 18) == rgb565(palette::BOARD_LIGHT)); // the cell is a light one
}

TEST_CASE("The bomb in the body is a ball on its piece, and the piece flashes pink near the tail", "[raster][potato]")
{
    const std::vector<Cell> snake{{10, 10}, {9, 10}, {8, 10}, {7, 10}, {6, 10}, {5, 10}, {4, 10}};
    const auto build = [&](std::size_t index, std::uint32_t ticks)
    {
        const auto game = Simulation::from_setup({.snake = snake,
                                                  .food = {2, 2},
                                                  .seed = 1,
                                                  .ticks = ticks,
                                                  .variant = Variant::HOT_POTATO,
                                                  .lump = snake[index]});
        REQUIRE(game.has_value());
        return *game;
    };
    Canvas far(CELL);
    paint_board(far.surface(), CELL, build(2, 0));
    CHECK(probe(far, CELL, {8, 10}, 16, 18) == rgb565(palette::INK));
    CHECK(probe(far, CELL, {8, 10}, 2, 14) == rgb565(palette::ACCENT)); // its stripe is its own

    Canvas near_even(CELL);
    paint_board(near_even.surface(), CELL, build(5, 2));
    Canvas near_odd(CELL);
    paint_board(near_odd.surface(), CELL, build(5, 3));
    CHECK(probe(near_even, CELL, {5, 10}, 2, 14) == rgb565(palette::TONGUE));
    CHECK(probe(near_odd, CELL, {5, 10}, 2, 14) != rgb565(palette::TONGUE));
}

TEST_CASE("The spark of the bomb in the body is gold, and pink when the look says so", "[raster][potato]")
{
    const std::vector<Cell> snake{{10, 10}, {9, 10}, {8, 10}, {7, 10}, {6, 10}, {5, 10}, {4, 10}};
    const auto game = Simulation::from_setup(
        {.snake = snake, .food = {2, 2}, .seed = 1, .variant = Variant::HOT_POTATO, .lump = snake[2]});
    REQUIRE(game.has_value());
    Canvas gold(CELL);
    paint_board(gold.surface(), CELL, *game);
    Canvas pink(CELL);
    paint_board(pink.surface(), CELL, *game, {.spark = true});
    CHECK(probe(gold, CELL, {8, 10}, 16, 1) == rgb565(palette::MEDAL_GOLD));
    CHECK(probe(pink, CELL, {8, 10}, 16, 1) == rgb565(palette::TONGUE));
}

TEST_CASE("A bomb near the tail of a snake that crashed does not flash", "[raster][potato]")
{
    const std::vector<Cell> snake{{10, 10}, {9, 10}, {8, 10}, {7, 10}, {6, 10}, {5, 10}, {4, 10}};
    // Two ticks played, so a snake that was still running would flash pink on this one.
    const auto game = Simulation::from_setup({.snake = snake,
                                              .food = {2, 2},
                                              .seed = 1,
                                              .ticks = 2,
                                              .running = false,
                                              .variant = Variant::HOT_POTATO,
                                              .lump = snake[5]});
    REQUIRE(game.has_value());
    Canvas canvas(CELL);
    paint_board(canvas.surface(), CELL, *game);
    CHECK(probe(canvas, CELL, {5, 10}, 2, 14) == rgb565(palette::SURFACE)); // its own stripe
    CHECK(probe(canvas, CELL, {5, 10}, 16, 1) == rgb565(palette::MEDAL_GOLD));
}

TEST_CASE("A game that was won is drawn with its eyes open", "[raster][snake]")
{
    // A snake that fills the board in rows, listed from the tail and then turned round to start at the head.
    std::vector<Cell> snake;
    for (std::uint8_t y = 0; y < snake::sim::BOARD_ROWS; ++y)
    {
        for (std::uint8_t step = 0; step < snake::sim::BOARD_COLUMNS; ++step)
        {
            snake.push_back({static_cast<std::uint8_t>(y % 2 == 0 ? step : snake::sim::BOARD_COLUMNS - 1 - step), y});
        }
    }
    std::ranges::reverse(snake);
    const auto game = Simulation::from_setup({.heading = snake::sim::Direction::LEFT,
                                              .snake = snake,
                                              .food = snake.front(),
                                              .seed = 1,
                                              .running = false,
                                              .won = true});
    REQUIRE(game.has_value());
    REQUIRE(game->won());
    Canvas canvas(CELL);
    paint_board(canvas.surface(), CELL, *game);
    // Facing left, the pupil of the eye on the lower side is a block of ink, which a cross would not make.
    CHECK(probe(canvas, CELL, game->head(), 9, 23) == rgb565(palette::INK));
}

TEST_CASE("A heading that is none of the four is drawn as facing right", "[raster][snake]")
{
    const std::vector<Cell> snake{{10, 10}, {9, 10}, {8, 10}};
    const auto odd = Simulation::from_setup(
        {.heading = static_cast<snake::sim::Direction>(4), .snake = snake, .food = {2, 2}, .seed = 1});
    REQUIRE(odd.has_value());
    Canvas odd_canvas(CELL);
    paint_board(odd_canvas.surface(), CELL, *odd);
    Canvas right_canvas(CELL);
    paint_board(right_canvas.surface(), CELL, game_with(snake, {2, 2}));
    CHECK(odd_canvas.pixels == right_canvas.pixels);
}

TEST_CASE("The painter writes only inside the board and leaves the rest of the surface alone", "[raster][safety]")
{
    for (const int cell : {8, 9, 12, 16, 24, 27, 32, 40})
    {
        INFO("cell " << cell);
        Canvas canvas(cell, 10);
        paint_board(canvas.surface(), cell, Simulation(snake::sim::EdgeRule::WALLS, 7, Variant::PORTALS));
        for (int y = 0; y < canvas.height; ++y)
        {
            for (int x = 0; x < canvas.width; ++x)
            {
                if (x >= canvas.side || y >= canvas.side)
                {
                    REQUIRE(canvas.at(x, y) == UNTOUCHED);
                }
            }
        }
    }
}

TEST_CASE("A surface that is too small, and a cell that is too small, are left alone", "[raster][safety]")
{
    const Simulation game(snake::sim::EdgeRule::WALLS, 3);
    Canvas small(CELL);
    small.height -= 1; // one row short
    small.pixels.assign(static_cast<std::size_t>(small.width) * static_cast<std::size_t>(small.height), UNTOUCHED);
    paint_board(small.surface(), CELL, game);
    CHECK(std::ranges::all_of(small.pixels, [](std::uint16_t pixel) { return pixel == UNTOUCHED; }));

    Canvas narrow(CELL);
    narrow.width -= 1; // one column short
    narrow.pixels.assign(static_cast<std::size_t>(narrow.width) * static_cast<std::size_t>(narrow.height), UNTOUCHED);
    paint_board(narrow.surface(), CELL, game);
    CHECK(std::ranges::all_of(narrow.pixels, [](std::uint16_t pixel) { return pixel == UNTOUCHED; }));

    Canvas tiny(7);
    paint_board(tiny.surface(), 7, game);
    CHECK(std::ranges::all_of(tiny.pixels, [](std::uint16_t pixel) { return pixel == UNTOUCHED; }));

    paint_board({}, CELL, game); // no pixels at all
    SUCCEED();
}

TEST_CASE("Painting the same game twice gives the same picture, and a game that moved gives another", "[raster]")
{
    Simulation game(snake::sim::EdgeRule::WALLS, 5);
    Canvas first(CELL);
    Canvas second(CELL);
    paint_board(first.surface(), CELL, game);
    paint_board(second.surface(), CELL, game);
    CHECK(first.pixels == second.pixels);
    static_cast<void>(game.turn(snake::sim::autoplay(game)));
    static_cast<void>(game.tick());
    Canvas moved(CELL);
    paint_board(moved.surface(), CELL, game);
    CHECK(first.pixels != moved.pixels);
}

TEST_CASE("Games of every variant play on and paint at every size without a pixel out of place", "[raster][properties]")
{
    for (const Variant variant : {Variant::PLAIN, Variant::PORTALS, Variant::HOT_POTATO})
    {
        Simulation game(snake::sim::EdgeRule::WALLS, 11, variant);
        for (int tick = 0; tick < 600 && game.running(); ++tick)
        {
            static_cast<void>(game.turn(snake::sim::autoplay(game)));
            static_cast<void>(game.shed());
            static_cast<void>(game.tick());
            if (tick % 40 == 0)
            {
                for (const int cell : {8, 13, 24})
                {
                    Canvas canvas(cell, 4);
                    paint_board(canvas.surface(), cell, game, {.lift = tick % 5, .spark = tick % 2 == 0});
                    REQUIRE(canvas.at(canvas.width - 1, canvas.height - 1) == UNTOUCHED);
                }
            }
        }
    }
}
