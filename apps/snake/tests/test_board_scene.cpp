// SPDX-License-Identifier: GPL-3.0-or-later
#include "board/board_scene.hpp"
#include "snake/sim/cell.hpp"
#include "snake/sim/direction.hpp"
#include "snake/sim/simulation.hpp"
#include "theme/tokens.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QPoint>
#include <QRect>
#include <QRgb>
#include <QSize>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace board_scene = snake::ui::board_scene;
namespace tokens = snake::ui::tokens;
using board_scene::Layout;
using board_scene::Shape;
using snake::sim::Cell;
using snake::sim::Direction;
using snake::sim::Simulation;

namespace
{

constexpr QRgb opaque(std::uint32_t rgb)
{
    return 0xFF000000U | rgb;
}

/// The layout of a board in an item 672 pixels square (the frame and the shadow around 640), which gives the 32 pixel
/// cells of the design.
Layout desktop_layout()
{
    return board_scene::layout_for(QSize(672, 672));
}

QRect cell_rect(const Layout& layout, Cell cell)
{
    return QRect(layout.origin.x() + (cell.x * layout.cell), layout.origin.y() + (cell.y * layout.cell), layout.cell,
                 layout.cell);
}

/// The colour a viewer sees at a pixel: that of the last shape that covers it, or 0 where nothing is drawn.
QRgb colour_at(const std::vector<Shape>& shapes, QPoint at)
{
    QRgb colour = 0;
    for (const Shape& shape : shapes)
    {
        if (shape.rect.contains(at))
        {
            colour = shape.colour;
        }
    }
    return colour;
}

/// A point in a cell, given in 32nds of the cell from its top left.
QPoint in_cell(const Layout& layout, Cell cell, int x, int y)
{
    const QRect rect = cell_rect(layout, cell);
    return QPoint(rect.left() + ((x * layout.cell) / 32), rect.top() + ((y * layout.cell) / 32));
}

std::size_t count_of(const std::vector<Shape>& shapes, QRgb colour)
{
    return static_cast<std::size_t>(
        std::count_if(shapes.begin(), shapes.end(), [colour](const Shape& shape) { return shape.colour == colour; }));
}

Simulation game_with(std::vector<Cell> snake, Direction heading, Cell food, std::uint32_t ticks = 0,
                     std::optional<Simulation::Bonus> bonus = std::nullopt)
{
    Simulation::Setup setup;
    setup.heading = heading;
    setup.snake = snake;
    setup.food = food;
    setup.ticks = ticks;
    setup.bonus = bonus;
    auto game = Simulation::from_setup(setup);
    REQUIRE(game.has_value());
    return *game;
}

/// A snake of three cells with its head at (10, 10), travelling in `heading`.
Simulation three_cells(Direction heading)
{
    switch (heading)
    {
    case Direction::UP:
        return game_with({{10, 10}, {10, 11}, {10, 12}}, heading, {3, 3});
    case Direction::DOWN:
        return game_with({{10, 10}, {10, 9}, {10, 8}}, heading, {3, 3});
    case Direction::LEFT:
        return game_with({{10, 10}, {11, 10}, {12, 10}}, heading, {3, 3});
    case Direction::RIGHT:
        break;
    }
    return game_with({{10, 10}, {9, 10}, {8, 10}}, heading, {3, 3});
}

} // namespace

TEST_CASE("The layout uses whole cells, fits the item and is centred", "[board-scene]")
{
    for (const QSize size : {QSize(400, 400), QSize(400, 200), QSize(200, 400), QSize(1280, 800), QSize(390, 700)})
    {
        INFO(size.width() << "x" << size.height());
        const Layout layout = board_scene::layout_for(size);
        REQUIRE(layout.cell > 0);
        const QRect outer = layout.outer();
        CHECK(outer.left() >= 0);
        CHECK(outer.top() >= 0);
        CHECK(outer.right() < size.width());
        CHECK(outer.bottom() + layout.drop < size.height());
        // The frame and its shadow are centred, to within a pixel.
        CHECK(std::abs(outer.left() - (size.width() - 1 - outer.right())) <= 1);
        CHECK(std::abs(outer.top() - (size.height() - 1 - (outer.bottom() + layout.drop))) <= 1);
        // No larger cell would fit.
        const Layout larger = board_scene::layout_for(QSize(size.width() + 20, size.height() + 20));
        CHECK(larger.cell >= layout.cell);
    }
}

TEST_CASE("The desktop layout has the cell, frame and shadow of the design", "[board-scene]")
{
    const Layout layout = desktop_layout();
    CHECK(layout.cell == 32);
    CHECK(layout.frame == 8);
    CHECK(layout.drop == 16);
    CHECK(layout.cells().size() == QSize(640, 640));
    CHECK(layout.pixel() == 2);
}

TEST_CASE("An item too small for the board has no layout and draws nothing", "[board-scene]")
{
    const Layout layout = board_scene::layout_for(QSize(20, 20));
    CHECK(layout.cell == 0);
    CHECK(board_scene::background_shapes(layout).empty());
    const Simulation game = three_cells(Direction::RIGHT);
    CHECK(board_scene::game_shapes(layout, game, 0.0).empty());
    CHECK(board_scene::layout_for(QSize(0, 0)).cell == 0);
}

TEST_CASE("The background is a shadow, a frame and a checkerboard", "[board-scene]")
{
    const Layout layout = desktop_layout();
    const std::vector<Shape> shapes = board_scene::background_shapes(layout);
    REQUIRE_FALSE(shapes.empty());

    // The frame is drawn above its shadow, and the shadow shows below it.
    const QRect outer = layout.outer();
    CHECK(colour_at(shapes, QPoint(outer.center().x(), outer.top() + 1)) == opaque(tokens::INK));
    CHECK(colour_at(shapes, QPoint(outer.center().x(), outer.bottom() + (layout.drop / 2))) == opaque(tokens::SHADOW));
    CHECK(colour_at(shapes, QPoint(outer.center().x(), outer.bottom() + layout.drop + 2)) == 0);
    // The corners of the frame are cut.
    CHECK(colour_at(shapes, outer.topLeft()) == 0);

    // The top left cell is dark and its neighbours are light.
    CHECK(colour_at(shapes, in_cell(layout, Cell{0, 0}, 16, 16)) == opaque(tokens::BOARD_DARK));
    CHECK(colour_at(shapes, in_cell(layout, Cell{1, 0}, 16, 16)) == opaque(tokens::BOARD_LIGHT));
    CHECK(colour_at(shapes, in_cell(layout, Cell{0, 1}, 16, 16)) == opaque(tokens::BOARD_LIGHT));
    CHECK(colour_at(shapes, in_cell(layout, Cell{1, 1}, 16, 16)) == opaque(tokens::BOARD_DARK));
    CHECK(count_of(shapes, opaque(tokens::BOARD_LIGHT)) == 200);

    for (const Shape& shape : shapes)
    {
        CHECK_FALSE(shape.rect.isEmpty());
    }
}

TEST_CASE("The head is the accent colour with white eyes, dark pupils and a tongue in front", "[board-scene]")
{
    const Layout layout = desktop_layout();
    for (const Direction heading : {Direction::RIGHT, Direction::DOWN, Direction::LEFT, Direction::UP})
    {
        INFO(static_cast<int>(heading));
        const Simulation game = three_cells(heading);
        const std::vector<Shape> shapes = board_scene::game_shapes(layout, game, 0.0);
        const QRect head = cell_rect(layout, Cell{10, 10});

        CHECK(colour_at(shapes, head.center()) == opaque(tokens::ACCENT));
        REQUIRE(count_of(shapes, opaque(tokens::EYE)) == 2);
        REQUIRE(count_of(shapes, opaque(tokens::INK)) == 2);
        REQUIRE(count_of(shapes, opaque(tokens::TONGUE)) == 1);

        // Which way is forward, as a step along x and y.
        const QPoint forward = heading == Direction::RIGHT  ? QPoint(1, 0)
                               : heading == Direction::LEFT ? QPoint(-1, 0)
                               : heading == Direction::DOWN ? QPoint(0, 1)
                                                            : QPoint(0, -1);
        const auto ahead_of_centre = [&](const QRect& rect)
        {
            const QPoint offset = rect.center() - head.center();
            return (offset.x() * forward.x()) + (offset.y() * forward.y());
        };
        for (const Shape& shape : shapes)
        {
            if (shape.colour == opaque(tokens::EYE))
            {
                CHECK(ahead_of_centre(shape.rect) > 0);
                CHECK(head.contains(shape.rect));
            }
            if (shape.colour == opaque(tokens::TONGUE))
            {
                // The tongue leaves the head's cell at the front, in line with the middle of the head.
                CHECK(ahead_of_centre(shape.rect) > layout.cell / 2);
                CHECK(layout.cells().contains(shape.rect));
            }
        }
    }
}

TEST_CASE("The eyes are on either side of the head and the pupils look ahead", "[board-scene]")
{
    const Layout layout = desktop_layout();
    const Simulation game = three_cells(Direction::RIGHT);
    const std::vector<Shape> shapes = board_scene::game_shapes(layout, game, 0.0);
    // The design for a snake facing right, in 32nds of a cell.
    CHECK(colour_at(shapes, in_cell(layout, Cell{10, 10}, 18, 6)) == opaque(tokens::EYE));
    CHECK(colour_at(shapes, in_cell(layout, Cell{10, 10}, 18, 22)) == opaque(tokens::EYE));
    CHECK(colour_at(shapes, in_cell(layout, Cell{10, 10}, 24, 8)) == opaque(tokens::INK));
    CHECK(colour_at(shapes, in_cell(layout, Cell{10, 10}, 24, 22)) == opaque(tokens::INK));
    CHECK(colour_at(shapes, in_cell(layout, Cell{10, 10}, 34, 15)) == opaque(tokens::TONGUE));
    CHECK(colour_at(shapes, in_cell(layout, Cell{10, 10}, 8, 16)) == opaque(tokens::ACCENT));
}

TEST_CASE("The tongue stops at the edge of the board", "[board-scene]")
{
    const Layout layout = desktop_layout();
    const Simulation at_edge = game_with({{19, 10}, {18, 10}, {17, 10}}, Direction::RIGHT, {3, 3});
    const std::vector<Shape> shapes = board_scene::game_shapes(layout, at_edge, 0.0);
    CHECK(count_of(shapes, opaque(tokens::TONGUE)) == 0);
    for (const Shape& shape : shapes)
    {
        CHECK(layout.outer().contains(shape.rect));
    }
}

TEST_CASE("The body is striped, and a stripe keeps its place in the snake as it moves", "[board-scene]")
{
    const Layout layout = desktop_layout();
    Simulation game = three_cells(Direction::RIGHT);
    std::vector<Shape> shapes = board_scene::game_shapes(layout, game, 0.0);
    CHECK(colour_at(shapes, in_cell(layout, Cell{9, 10}, 16, 10)) == opaque(tokens::SURFACE));
    CHECK(colour_at(shapes, in_cell(layout, Cell{8, 10}, 16, 10)) == opaque(tokens::ACCENT));

    for (int tick = 0; tick < 3; ++tick)
    {
        REQUIRE(game.tick() != snake::sim::TickOutcome::HIT_WALL);
        shapes = board_scene::game_shapes(layout, game, 0.0);
        const auto& snake = game.snake();
        REQUIRE(snake.size() == 3);
        CHECK(colour_at(shapes, in_cell(layout, snake[1], 16, 10)) == opaque(tokens::SURFACE));
        CHECK(colour_at(shapes, in_cell(layout, snake[2], 16, 10)) == opaque(tokens::ACCENT));
    }
}

TEST_CASE("A segment has a darker lower edge", "[board-scene]")
{
    const Layout layout = desktop_layout();
    const Simulation game = three_cells(Direction::RIGHT);
    const std::vector<Shape> shapes = board_scene::game_shapes(layout, game, 0.0);
    const QRgb top = colour_at(shapes, in_cell(layout, Cell{9, 10}, 16, 8));
    const QRgb bottom = colour_at(shapes, in_cell(layout, Cell{9, 10}, 16, 30));
    REQUIRE(top == opaque(tokens::SURFACE));
    CHECK(bottom != top);
    CHECK(qRed(bottom) < qRed(top));
    CHECK(qGreen(bottom) < qGreen(top));
    CHECK(qBlue(bottom) < qBlue(top));
}

TEST_CASE("The food is a blue berry with a highlight and a leaf, and it bobs", "[board-scene]")
{
    const Layout layout = desktop_layout();
    const Cell food{5, 5};
    const Simulation game = game_with({{10, 10}, {9, 10}, {8, 10}}, Direction::RIGHT, food);
    const std::vector<Shape> resting = board_scene::game_shapes(layout, game, 0.0);

    CHECK(colour_at(resting, in_cell(layout, food, 16, 16)) == opaque(tokens::FOOD));
    CHECK(colour_at(resting, in_cell(layout, food, 11, 11)) == opaque(tokens::FOOD_HIGHLIGHT));
    CHECK(colour_at(resting, in_cell(layout, food, 15, 0)) == opaque(tokens::LEAF));
    // The corners of the cell are empty, and the lower right of the berry is in shade.
    CHECK(colour_at(resting, in_cell(layout, food, 1, 1)) == 0);
    const QRgb shade = colour_at(resting, in_cell(layout, food, 22, 22));
    CHECK(shade != 0);
    CHECK(shade != opaque(tokens::FOOD));
    CHECK(qBlue(shade) < qBlue(opaque(tokens::FOOD)));

    // At the top of the bob, everything of the food is five 32nds of a cell higher, and nothing else has moved.
    const std::vector<Shape> raised = board_scene::game_shapes(layout, game, 1.0);
    REQUIRE(raised.size() == resting.size());
    const int rise = 5;
    for (std::size_t index = 0; index < resting.size(); ++index)
    {
        const bool is_food = resting[index].colour == opaque(tokens::FOOD_HIGHLIGHT) ||
                             resting[index].colour == opaque(tokens::LEAF) ||
                             resting[index].colour == opaque(tokens::FOOD);
        const int moved = resting[index].rect.top() - raised[index].rect.top();
        CHECK(moved == (is_food || resting[index].rect.top() < layout.origin.y() + (6 * layout.cell) ? rise : 0));
    }

    // A lift beyond the ends is held to them.
    CHECK(board_scene::game_shapes(layout, game, 7.0).front().rect == raised.front().rect);
    CHECK(board_scene::game_shapes(layout, game, -3.0).front().rect == resting.front().rect);
}

TEST_CASE("The bonus item is gold and bobs with the food", "[board-scene]")
{
    const Layout layout = desktop_layout();
    const Cell bonus{15, 4};
    const Simulation game =
        game_with({{10, 10}, {9, 10}, {8, 10}}, Direction::RIGHT, {3, 3}, 0, Simulation::Bonus{bonus, 20});
    const std::vector<Shape> resting = board_scene::game_shapes(layout, game, 0.0);
    const std::vector<Shape> raised = board_scene::game_shapes(layout, game, 1.0);
    CHECK(colour_at(resting, in_cell(layout, bonus, 16, 16)) == opaque(tokens::MEDAL_GOLD));
    CHECK(colour_at(raised, in_cell(layout, bonus, 16, 16 - 5)) == opaque(tokens::MEDAL_GOLD));
    // Only the food has a leaf.
    CHECK(colour_at(resting, in_cell(layout, bonus, 15, 0)) == 0);
    CHECK(colour_at(resting, in_cell(layout, Cell{3, 3}, 15, 0)) == opaque(tokens::LEAF));
}

TEST_CASE("A won game has no food", "[board-scene]")
{
    // The snake fills the board: one row after another, each the other way round.
    std::vector<Cell> path;
    for (std::uint8_t y = 0; y < snake::sim::BOARD_ROWS; ++y)
    {
        for (std::uint8_t step = 0; step < snake::sim::BOARD_COLUMNS; ++step)
        {
            path.push_back(
                Cell{static_cast<std::uint8_t>(y % 2 == 0 ? step : snake::sim::BOARD_COLUMNS - 1 - step), y});
        }
    }
    std::vector<Cell> head_first(path.rbegin(), path.rend());
    Simulation::Setup setup;
    setup.heading = Direction::LEFT;
    setup.snake = head_first;
    setup.food = head_first.front();
    setup.running = false;
    setup.won = true;
    const auto game = Simulation::from_setup(setup);
    REQUIRE(game.has_value());
    REQUIRE(game->won());

    const std::vector<Shape> shapes = board_scene::game_shapes(desktop_layout(), *game, 0.0);
    CHECK(count_of(shapes, opaque(tokens::FOOD)) == 0);
    CHECK(count_of(shapes, opaque(tokens::LEAF)) == 0);
    CHECK(count_of(shapes, opaque(tokens::EYE)) == 2);
}

TEST_CASE("Small cells still draw every part of the art", "[board-scene]")
{
    const Layout layout = board_scene::layout_for(QSize(160, 160));
    REQUIRE(layout.cell > 0);
    REQUIRE(layout.cell < 8);
    const Simulation game = three_cells(Direction::RIGHT);
    const std::vector<Shape> shapes = board_scene::game_shapes(layout, game, 1.0);
    CHECK(count_of(shapes, opaque(tokens::EYE)) == 2);
    CHECK(count_of(shapes, opaque(tokens::TONGUE)) == 1);
    CHECK(count_of(shapes, opaque(tokens::LEAF)) == 1);
    for (const Shape& shape : shapes)
    {
        CHECK_FALSE(shape.rect.isEmpty());
    }
}

namespace
{

/// The left edge of the rectangle of `colour` that is nearest the left, which for the head's eye is the head's left.
int left_of(const std::vector<Shape>& shapes, QRgb colour)
{
    int left = 1'000'000;
    for (const Shape& shape : shapes)
    {
        if (shape.colour == colour)
        {
            left = std::min(left, shape.rect.left());
        }
    }
    return left;
}

} // namespace

TEST_CASE("Between ticks the snake glides into the cells ahead of it", "[board-scene]")
{
    const Layout layout = desktop_layout();
    const Simulation game = three_cells(Direction::RIGHT);
    const std::vector<Shape> still = board_scene::game_shapes(layout, game, 0.0, 0.0);
    const std::vector<Shape> half = board_scene::game_shapes(layout, game, 0.0, 0.5);
    const std::vector<Shape> done = board_scene::game_shapes(layout, game, 0.0, 1.0);

    // The head's eye is half a cell further right, and a whole cell at the end of the tick.
    const int eye = left_of(still, opaque(tokens::EYE));
    CHECK(left_of(half, opaque(tokens::EYE)) == eye + (layout.cell / 2));
    CHECK(left_of(done, opaque(tokens::EYE)) == eye + layout.cell);

    // The tail has left half of its cell, and the cell behind it is empty.
    CHECK(colour_at(half, in_cell(layout, Cell{8, 10}, 4, 16)) == 0);
    CHECK(colour_at(half, in_cell(layout, Cell{8, 10}, 24, 10)) == opaque(tokens::ACCENT));
    CHECK(colour_at(half, in_cell(layout, Cell{9, 10}, 4, 10)) == opaque(tokens::ACCENT));
    CHECK(colour_at(half, in_cell(layout, Cell{9, 10}, 20, 10)) == opaque(tokens::SURFACE));

    // At the end of the tick the picture is the one the next tick draws at its start, so nothing jumps.
    Simulation next = game;
    REQUIRE(next.tick() == snake::sim::TickOutcome::MOVED);
    const std::vector<Shape> after = board_scene::game_shapes(layout, next, 0.0, 0.0);
    for (const Cell cell : {Cell{11, 10}, Cell{10, 10}, Cell{9, 10}, Cell{8, 10}})
    {
        INFO(int(cell.x));
        CHECK(colour_at(done, in_cell(layout, cell, 16, 10)) == colour_at(after, in_cell(layout, cell, 16, 10)));
    }
    CHECK(left_of(done, opaque(tokens::EYE)) == left_of(after, opaque(tokens::EYE)));
    // The progress is held between 0 and 1.
    CHECK(left_of(board_scene::game_shapes(layout, game, 0.0, 3.0), opaque(tokens::EYE)) ==
          left_of(done, opaque(tokens::EYE)));
    CHECK(left_of(board_scene::game_shapes(layout, game, 0.0, -1.0), opaque(tokens::EYE)) == eye);
}

TEST_CASE("A turn that is waiting is drawn as the head turns into it", "[board-scene]")
{
    const Layout layout = desktop_layout();
    Simulation game = three_cells(Direction::RIGHT);
    REQUIRE(game.turn(Direction::DOWN));
    const std::vector<Shape> shapes = board_scene::game_shapes(layout, game, 0.0, 0.5);

    // The head moves down into (10, 11) and faces down; the segment behind it moves right into the head's cell.
    const QRect head = QRect(layout.origin.x() + (10 * layout.cell),
                             layout.origin.y() + (10 * layout.cell) + (layout.cell / 2), layout.cell, layout.cell);
    CHECK(colour_at(shapes, head.center()) == opaque(tokens::ACCENT));
    // The tongue is below the head.
    CHECK(colour_at(shapes, QPoint(head.center().x(), head.bottom() + 2)) == opaque(tokens::TONGUE));
    CHECK(colour_at(shapes, in_cell(layout, Cell{9, 10}, 28, 16)) == opaque(tokens::SURFACE));
}

TEST_CASE("The snake does not glide into a wall, into itself or across the edge, or when the game is not running",
          "[board-scene]")
{
    const Layout layout = desktop_layout();
    const auto eye_at = [&](const Simulation& game, double progress)
    { return left_of(board_scene::game_shapes(layout, game, 0.0, progress), opaque(tokens::EYE)); };

    const Simulation wall = game_with({{19, 10}, {18, 10}, {17, 10}}, Direction::RIGHT, {3, 3});
    CHECK(eye_at(wall, 0.8) == eye_at(wall, 0.0));

    // A snake curled so that its next cell is its own body.
    const Simulation curled = game_with({{5, 5}, {5, 6}, {6, 6}, {6, 5}, {6, 4}}, Direction::RIGHT, {3, 3});
    CHECK(eye_at(curled, 0.8) == eye_at(curled, 0.0));
    // The same snake with its tail on that cell, which moves away, glides.
    const Simulation chasing = game_with({{5, 5}, {5, 6}, {6, 6}, {6, 5}}, Direction::RIGHT, {3, 3});
    CHECK(eye_at(chasing, 0.8) > eye_at(chasing, 0.0));

    Simulation::Setup stopped;
    stopped.heading = Direction::RIGHT;
    const std::array<Cell, 3> cells = {{{10, 10}, {9, 10}, {8, 10}}};
    stopped.snake = cells;
    stopped.food = Cell{3, 3};
    stopped.running = false;
    const auto over = Simulation::from_setup(stopped);
    REQUIRE(over.has_value());
    CHECK(eye_at(*over, 0.8) == eye_at(*over, 0.0));
}

TEST_CASE("A snake that is about to eat keeps its tail and grows a piece behind it", "[board-scene]")
{
    const Layout layout = desktop_layout();
    const Simulation game = game_with({{10, 10}, {9, 10}, {8, 10}}, Direction::RIGHT, {11, 10});
    const std::vector<Shape> half = board_scene::game_shapes(layout, game, 0.0, 0.5);

    // The tail's cell is never empty: the tail has moved half a cell on, and the new piece shows behind it.
    CHECK(colour_at(half, in_cell(layout, Cell{8, 10}, 4, 16)) == opaque(tokens::SURFACE));
    CHECK(colour_at(half, in_cell(layout, Cell{8, 10}, 24, 10)) == opaque(tokens::ACCENT));

    // When the tick comes the body is that long.
    Simulation next = game;
    REQUIRE(next.tick() == snake::sim::TickOutcome::ATE);
    CHECK(next.snake().size() == 4);
    const std::vector<Shape> after = board_scene::game_shapes(layout, next, 0.0, 0.0);
    const std::vector<Shape> done = board_scene::game_shapes(layout, game, 0.0, 1.0);
    for (const Cell cell : {Cell{11, 10}, Cell{10, 10}, Cell{9, 10}, Cell{8, 10}})
    {
        CHECK(colour_at(done, in_cell(layout, cell, 16, 10)) == colour_at(after, in_cell(layout, cell, 16, 10)));
    }
}

TEST_CASE("A snake that is about to eat the bonus item grows as well", "[board-scene]")
{
    const Layout layout = desktop_layout();
    const Simulation game =
        game_with({{10, 10}, {9, 10}, {8, 10}}, Direction::RIGHT, {3, 3}, 0, Simulation::Bonus{Cell{11, 10}, 20});
    const std::vector<Shape> half = board_scene::game_shapes(layout, game, 0.0, 0.5);
    CHECK(colour_at(half, in_cell(layout, Cell{8, 10}, 4, 16)) == opaque(tokens::SURFACE));
}

namespace
{

/// A running game with the snake heading right from (10, 10) and the food at `food`.
Simulation heading_right_with_food(Cell food)
{
    return game_with({Cell{10, 10}, Cell{9, 10}, Cell{8, 10}}, Direction::RIGHT, food);
}

bool mouth_is_open(const Simulation& game)
{
    const Layout layout = desktop_layout();
    const auto shapes = board_scene::game_shapes(layout, game, 0.0);
    // An open mouth takes the place of the tongue.
    return count_of(shapes, opaque(tokens::TONGUE)) == 0;
}

} // namespace

TEST_CASE("The head opens its mouth when food is close ahead", "[board-scene][food]")
{
    // A tongue as long as nothing is near, for food far ahead, behind the head, or off to one side.
    CHECK_FALSE(mouth_is_open(heading_right_with_food(Cell{18, 10})));
    CHECK_FALSE(mouth_is_open(heading_right_with_food(Cell{2, 10})));
    CHECK_FALSE(mouth_is_open(heading_right_with_food(Cell{10, 15})));
    CHECK_FALSE(mouth_is_open(heading_right_with_food(Cell{12, 13})));

    // From one to three cells ahead and no more than one off to the side, the mouth is open.
    CHECK(mouth_is_open(heading_right_with_food(Cell{11, 10})));
    CHECK(mouth_is_open(heading_right_with_food(Cell{12, 10})));
    CHECK(mouth_is_open(heading_right_with_food(Cell{13, 10})));
    CHECK_FALSE(mouth_is_open(heading_right_with_food(Cell{14, 10})));
    CHECK(mouth_is_open(heading_right_with_food(Cell{12, 9})));
    CHECK(mouth_is_open(heading_right_with_food(Cell{12, 11})));
    CHECK_FALSE(mouth_is_open(heading_right_with_food(Cell{12, 12})));
}

TEST_CASE("The mouth opens towards the way the head faces, whichever way that is", "[board-scene][food]")
{
    const auto going = [](Direction heading, std::vector<Cell> snake, Cell food)
    { return game_with(std::move(snake), heading, food); };
    CHECK(mouth_is_open(going(Direction::LEFT, {Cell{10, 10}, Cell{11, 10}, Cell{12, 10}}, Cell{8, 10})));
    CHECK_FALSE(mouth_is_open(going(Direction::LEFT, {Cell{10, 10}, Cell{11, 10}, Cell{12, 10}}, Cell{12, 9})));
    CHECK(mouth_is_open(going(Direction::DOWN, {Cell{10, 10}, Cell{10, 9}, Cell{10, 8}}, Cell{10, 12})));
    CHECK_FALSE(mouth_is_open(going(Direction::DOWN, {Cell{10, 10}, Cell{10, 9}, Cell{10, 8}}, Cell{10, 5})));
    CHECK(mouth_is_open(going(Direction::UP, {Cell{10, 10}, Cell{10, 11}, Cell{10, 12}}, Cell{9, 8})));
    CHECK_FALSE(mouth_is_open(going(Direction::UP, {Cell{10, 10}, Cell{10, 11}, Cell{10, 12}}, Cell{10, 13})));
}

TEST_CASE("The mouth is dark, between the eyes, at the front of the head", "[board-scene][food]")
{
    const Layout layout = desktop_layout();
    const auto near_food = board_scene::game_shapes(layout, heading_right_with_food(Cell{12, 10}), 0.0);
    // The front of the head (the right side, in the middle) is ink where the tongue root would be.
    CHECK(colour_at(near_food, in_cell(layout, Cell{10, 10}, 27, 16)) == opaque(tokens::INK));
    const auto far_food = board_scene::game_shapes(layout, heading_right_with_food(Cell{18, 10}), 0.0);
    CHECK(colour_at(far_food, in_cell(layout, Cell{10, 10}, 27, 16)) == opaque(tokens::ACCENT));
}
