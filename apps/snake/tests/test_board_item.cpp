// SPDX-License-Identifier: GPL-3.0-or-later
#include "board/board_item.hpp"
#include "board/board_scene.hpp"
#include "snake/sim/autoplayer.hpp"
#include "snake/sim/cell.hpp"
#include "snake/sim/direction.hpp"
#include "theme/tokens.hpp"
#include "viewmodel/game_view_model.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QColor>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QImage>
#include <QObject>
#include <QQuickWindow>
#include <QRgb>
#include <QSize>
#include <Qt>
#include <cstdint>
#include <memory>

using snake::sim::Cell;
using snake::ui::BoardItem;
using snake::ui::GameViewModel;
namespace board_scene = snake::ui::board_scene;
namespace tokens = snake::ui::tokens;

namespace
{

constexpr QRgb WINDOW_COLOUR = 0xFF000000;

constexpr QRgb opaque(std::uint32_t rgb)
{
    return 0xFF000000U | rgb;
}

GameViewModel::SeedSource counting_seeds(std::uint64_t first = 1)
{
    return [next = first]() mutable { return next++; };
}

/// A window with a board item that fills it, drawn without a display.
class Scene
{
public:
    explicit Scene(QSize size = QSize(400, 400))
    {
        m_Window.setColor(QColor::fromRgba(WINDOW_COLOUR));
        m_Window.resize(size);
        m_Board = new BoardItem(m_Window.contentItem());
        m_Board->setSize(size);
    }

    BoardItem& board()
    {
        return *m_Board;
    }

    /// Draws the window now and returns what it looks like.
    QImage render()
    {
        m_Window.show();
        QElapsedTimer elapsed;
        elapsed.start();
        while (!m_Window.isExposed() && elapsed.elapsed() < 5000)
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }
        return m_Window.grabWindow();
    }

private:
    QQuickWindow m_Window;
    BoardItem* m_Board = nullptr; // owned by the window's content item
};

/// The colour in the middle of a cell, where the board is laid out as the item lays it out for the image's size.
QRgb colour_of(const QImage& image, Cell cell)
{
    const board_scene::Layout layout = board_scene::layout_for(image.size());
    return image.pixel(layout.origin.x() + (cell.x * layout.cell) + (layout.cell / 2),
                       layout.origin.y() + (cell.y * layout.cell) + (layout.cell / 2));
}

/// The colour the checkerboard has in a cell: the light squares are where the column and the row add up to an odd
/// number.
QRgb checker_colour(Cell cell)
{
    return opaque(((cell.x + cell.y) % 2 == 1) ? tokens::BOARD_LIGHT : tokens::BOARD_DARK);
}

/// A cell that is neither the snake's nor the food's, to check the board's own colour.
Cell empty_cell(const GameViewModel& model)
{
    const auto& game = model.session().simulation();
    for (std::uint8_t x = 0; x < snake::sim::BOARD_COLUMNS; ++x)
    {
        const Cell cell{x, 0};
        if (game.food() != cell)
        {
            return cell;
        }
    }
    return Cell{0, 1};
}

} // namespace

TEST_CASE("A board with no game draws the frame and the checkerboard only", "[board]")
{
    Scene scene;
    const QImage image = scene.render();
    REQUIRE(image.width() == 400);
    CHECK(image.pixel(0, 0) == WINDOW_COLOUR);
    CHECK(colour_of(image, Cell{0, 0}) == checker_colour(Cell{0, 0}));
    CHECK(colour_of(image, Cell{10, 10}) == checker_colour(Cell{10, 10}));
    CHECK(colour_of(image, Cell{11, 10}) == checker_colour(Cell{11, 10}));
}

TEST_CASE("A new game shows the snake, the food and the board", "[board]")
{
    GameViewModel model(counting_seeds());
    Scene scene;
    scene.board().setGame(&model);
    const QImage image = scene.render();

    // The snake starts at the middle of the board heading right, with its body trailing to the left.
    CHECK(colour_of(image, Cell{10, 10}) == opaque(tokens::ACCENT));
    CHECK(colour_of(image, Cell{9, 10}) == opaque(tokens::SURFACE));
    CHECK(colour_of(image, Cell{8, 10}) == opaque(tokens::ACCENT));
    CHECK(colour_of(image, model.session().simulation().food()) == opaque(tokens::FOOD));
    CHECK(colour_of(image, empty_cell(model)) == checker_colour(empty_cell(model)));
}

TEST_CASE("The board is the largest square that fits, in the middle", "[board]")
{
    GameViewModel model(counting_seeds());
    Scene scene(QSize(400, 200));
    scene.board().setGame(&model);
    const QImage image = scene.render();

    const board_scene::Layout layout = board_scene::layout_for(QSize(400, 200));
    REQUIRE(layout.cell > 0);
    CHECK(layout.cell * snake::sim::BOARD_ROWS <= 200);
    CHECK(image.pixel(50, 100) == WINDOW_COLOUR);
    CHECK(image.pixel(350, 100) == WINDOW_COLOUR);
    CHECK(colour_of(image, Cell{10, 10}) == opaque(tokens::ACCENT));
}

TEST_CASE("The board follows the game", "[board]")
{
    GameViewModel model(counting_seeds());
    Scene scene;
    scene.board().setGame(&model);
    model.start();
    static_cast<void>(scene.render());

    model.advance(0.15);
    const QImage image = scene.render();
    REQUIRE(model.session().simulation().head() == Cell{11, 10});
    CHECK(colour_of(image, Cell{11, 10}) == opaque(tokens::ACCENT));
    // The stripes keep their places in the snake as it moves.
    CHECK(colour_of(image, Cell{10, 10}) == opaque(tokens::SURFACE));
    CHECK(colour_of(image, Cell{9, 10}) == opaque(tokens::ACCENT));
    // The tail has moved on, so its cell is empty again (unless the food is there).
    if (model.session().simulation().food() != Cell{8, 10})
    {
        CHECK(colour_of(image, Cell{8, 10}) == checker_colour(Cell{8, 10}));
    }
}

TEST_CASE("The bonus item is drawn when there is one", "[board]")
{
    GameViewModel model(counting_seeds(5));
    Scene scene;
    scene.board().setGame(&model);
    model.start();
    // The autoplayer steers with the arrow keys until a bonus item appears, which it does after five foods.
    for (int tick = 0; tick < 3000 && !model.session().simulation().bonus(); ++tick)
    {
        const auto direction = snake::sim::autoplay(model.session().simulation());
        const int key = direction == snake::sim::Direction::UP     ? Qt::Key_Up
                        : direction == snake::sim::Direction::DOWN ? Qt::Key_Down
                        : direction == snake::sim::Direction::LEFT ? Qt::Key_Left
                                                                   : Qt::Key_Right;
        model.handleKey(key, false);
        model.advance(0.15);
    }
    REQUIRE(model.session().simulation().bonus());
    const QImage image = scene.render();
    CHECK(colour_of(image, model.session().simulation().bonus()->cell) == opaque(tokens::MEDAL_GOLD));
}

TEST_CASE("Changing the game redraws, and a game that goes away leaves an empty board", "[board]")
{
    auto first = std::make_unique<GameViewModel>(counting_seeds());
    GameViewModel second(counting_seeds(2));
    Scene scene;
    int changes = 0;
    QObject::connect(&scene.board(), &BoardItem::gameChanged, [&changes] { ++changes; });

    scene.board().setGame(first.get());
    scene.board().setGame(first.get());
    CHECK(changes == 1);
    CHECK(scene.board().game() == first.get());
    CHECK(colour_of(scene.render(), Cell{10, 10}) == opaque(tokens::ACCENT));

    scene.board().setGame(&second);
    CHECK(changes == 2);
    // The old game's signals no longer reach the board.
    first->newGame();
    CHECK(colour_of(scene.render(), second.session().simulation().food()) == opaque(tokens::FOOD));

    first.reset();
    scene.board().setGame(nullptr);
    CHECK(changes == 3);
    CHECK(colour_of(scene.render(), Cell{10, 10}) == checker_colour(Cell{10, 10}));
}

TEST_CASE("A game that is deleted while the board shows it leaves an empty board", "[board]")
{
    auto model = std::make_unique<GameViewModel>(counting_seeds());
    Scene scene;
    scene.board().setGame(model.get());
    REQUIRE(colour_of(scene.render(), Cell{10, 10}) == opaque(tokens::ACCENT));
    model.reset();
    scene.board().update();
    CHECK(scene.board().game() == nullptr);
    CHECK(colour_of(scene.render(), Cell{10, 10}) == checker_colour(Cell{10, 10}));
}

TEST_CASE("Raising the food redraws it higher, and the lift is held between 0 and 1", "[board]")
{
    GameViewModel model(counting_seeds());
    Scene scene;
    scene.board().setGame(&model);
    int changes = 0;
    QObject::connect(&scene.board(), &BoardItem::foodLiftChanged, [&changes] { ++changes; });
    const QImage resting = scene.render();
    CHECK(scene.board().foodLift() == 0.0);

    scene.board().setFoodLift(0.0);
    CHECK(changes == 0);
    scene.board().setFoodLift(5.0);
    CHECK(changes == 1);
    CHECK(scene.board().foodLift() == 1.0);
    scene.board().setFoodLift(1.0);
    CHECK(changes == 1);
    const QImage raised = scene.render();
    CHECK(raised != resting);

    // The berry has risen into a pixel near the top of its cell, which is empty when the berry rests.
    const board_scene::Layout layout = board_scene::layout_for(raised.size());
    const Cell food = model.session().simulation().food();
    const int top = layout.origin.y() + (food.y * layout.cell);
    const int left = layout.origin.x() + (food.x * layout.cell);
    const int x = left + (layout.cell / 2);
    const int y_high = top + 1;
    CHECK(raised.pixel(x, y_high) == opaque(tokens::FOOD));
    CHECK(resting.pixel(x, y_high) != opaque(tokens::FOOD));

    scene.board().setFoodLift(-2.0);
    CHECK(scene.board().foodLift() == 0.0);
    CHECK(scene.render() == resting);
}

TEST_CASE("The snake moves between the cells while time passes within a tick", "[board]")
{
    GameViewModel model(counting_seeds());
    Scene scene;
    scene.board().setGame(&model);
    model.start();
    const QImage at_rest = scene.render();

    // Half of the first interval: no tick has run, and the head has moved half a cell on.
    model.advance(0.075);
    REQUIRE(model.session().simulation().ticks() == 0);
    const QImage between = scene.render();
    CHECK(between != at_rest);
    const board_scene::Layout layout = board_scene::layout_for(between.size());
    const int middle_y = layout.origin.y() + (10 * layout.cell) + (layout.cell / 2);
    const int next_cell_start = layout.origin.x() + (11 * layout.cell);
    CHECK(at_rest.pixel(next_cell_start + 1, middle_y) != opaque(tokens::ACCENT));
    CHECK(between.pixel(next_cell_start + 1, middle_y) == opaque(tokens::ACCENT));
}

namespace
{

/// The colour on `shapes` at a point: the last shape that covers it, or nothing.
std::optional<QRgb> colour_at(const std::vector<board_scene::Shape>& shapes, QPoint at)
{
    std::optional<QRgb> colour;
    for (const board_scene::Shape& shape : shapes)
    {
        if (shape.rect.contains(at))
        {
            colour = shape.colour;
        }
    }
    return colour;
}

/// The middle of a cell.
QPoint middle_of(const board_scene::Layout& layout, Cell cell)
{
    return QPoint(layout.origin.x() + (cell.x * layout.cell) + (layout.cell / 2),
                  layout.origin.y() + (cell.y * layout.cell) + (layout.cell / 2));
}

/// A point in the ring of a portal: on the middle row of the cell, in the left part.
QPoint ring_of(const board_scene::Layout& layout, Cell cell)
{
    return QPoint(layout.origin.x() + (cell.x * layout.cell) + (layout.cell / 8),
                  layout.origin.y() + (cell.y * layout.cell) + (layout.cell / 2));
}

const snake::sim::Simulation::PortalCells PORTAL_CELLS{{Cell{3, 3}, Cell{15, 4}, Cell{4, 15}, Cell{16, 16}}};

std::optional<snake::sim::Simulation> portal_game_for_drawing()
{
    const std::vector<Cell> snake{{10, 10}, {9, 10}, {8, 10}};
    return snake::sim::Simulation::from_setup(
        {.snake = snake, .food = {6, 7}, .seed = 1, .variant = snake::sim::Variant::PORTALS, .portals = PORTAL_CELLS});
}

} // namespace

TEST_CASE("The portals are drawn as rings round a dark hole, one colour for each pair", "[board][portals]")
{
    const auto game = portal_game_for_drawing();
    REQUIRE(game.has_value());
    const board_scene::Layout layout = board_scene::layout_for(QSize(480, 480));
    const std::vector<board_scene::Shape> shapes = board_scene::game_shapes(layout, *game, 0.0);
    for (std::size_t index = 0; index < PORTAL_CELLS.cells.size(); ++index)
    {
        const bool first_pair = index < 2;
        INFO("portal " << index);
        CHECK(colour_at(shapes, ring_of(layout, PORTAL_CELLS.cells[index])) ==
              opaque(first_pair ? tokens::TONGUE : tokens::MEDAL_SILVER));
        // The hole is the ink of the frame, not the board; the first pair has a dot in the middle of it.
        const std::optional<QRgb> middle = colour_at(shapes, middle_of(layout, PORTAL_CELLS.cells[index]));
        REQUIRE(middle.has_value());
        CHECK(*middle == opaque(first_pair ? tokens::TONGUE : tokens::INK));
    }
    // A cell without a portal has nothing drawn on it.
    CHECK_FALSE(colour_at(shapes, middle_of(layout, Cell{1, 1})).has_value());
}

TEST_CASE("The hole of a portal breathes with the bob, and stays inside its cell", "[board][portals]")
{
    const auto game = portal_game_for_drawing();
    REQUIRE(game.has_value());
    const board_scene::Layout layout = board_scene::layout_for(QSize(480, 480));
    const QRect cell(layout.origin.x() + (3 * layout.cell), layout.origin.y() + (3 * layout.cell), layout.cell,
                     layout.cell);
    for (const double lift : {0.0, 0.5, 1.0})
    {
        for (const board_scene::Shape& shape : board_scene::game_shapes(layout, *game, lift))
        {
            if (shape.colour == opaque(tokens::TONGUE) && shape.rect.intersects(cell))
            {
                CHECK(cell.contains(shape.rect));
            }
        }
    }
}

TEST_CASE("A head about to go through a portal does not glide towards it", "[board][portals]")
{
    // The head is next to the portal at (11, 10), heading right.
    const snake::sim::Simulation::PortalCells beside{{Cell{11, 10}, Cell{15, 4}, Cell{4, 15}, Cell{16, 16}}};
    const std::vector<Cell> snake{{10, 10}, {9, 10}, {8, 10}};
    const auto game = snake::sim::Simulation::from_setup(
        {.snake = snake, .food = {6, 7}, .seed = 1, .variant = snake::sim::Variant::PORTALS, .portals = beside});
    REQUIRE(game.has_value());
    const board_scene::Layout layout = board_scene::layout_for(QSize(480, 480));
    const auto at_start = board_scene::game_shapes(layout, *game, 0.0, 0.0);
    const auto halfway = board_scene::game_shapes(layout, *game, 0.0, 0.5);
    // The snake is where it was: nothing moves between the two pictures but nothing else changes either.
    const QPoint head = middle_of(layout, Cell{10, 10});
    CHECK(colour_at(at_start, head) == colour_at(halfway, head));
    CHECK(colour_at(halfway, QPoint(head.x() + (layout.cell / 2) + 1, head.y())) !=
          std::optional<QRgb>(opaque(tokens::ACCENT)));
}

TEST_CASE("The bomb food is an ink ball in a cream rim, and it blinks when it is about to go", "[board][potato]")
{
    const std::vector<Cell> snake{{10, 10}, {9, 10}, {8, 10}, {7, 10}, {6, 10}, {5, 10}};
    const auto build = [&](std::uint8_t left)
    {
        return snake::sim::Simulation::from_setup(
            {.snake = snake,
             .food = {2, 2},
             .seed = 1,
             .variant = snake::sim::Variant::HOT_POTATO,
             .potato = snake::sim::Simulation::Potato{.cell = {14, 5}, .ticks_left = left}});
    };
    const board_scene::Layout layout = board_scene::layout_for(QSize(480, 480));
    const QPoint centre = middle_of(layout, Cell{14, 5});

    const auto fresh = build(40);
    REQUIRE(fresh.has_value());
    CHECK(colour_at(board_scene::game_shapes(layout, *fresh, 0.0), centre) == opaque(tokens::INK));

    // Under ten ticks it is drawn every other two ticks.
    const auto lit = build(8);
    const auto dark = build(7);
    REQUIRE(lit.has_value());
    REQUIRE(dark.has_value());
    CHECK(colour_at(board_scene::game_shapes(layout, *lit, 0.0), centre) == opaque(tokens::INK));
    CHECK_FALSE(colour_at(board_scene::game_shapes(layout, *dark, 0.0), centre).has_value());
}

TEST_CASE("The bomb in the body is drawn on its piece, which flashes pink as it nears the tail", "[board][potato]")
{
    const std::vector<Cell> snake{{10, 10}, {9, 10}, {8, 10}, {7, 10}, {6, 10}, {5, 10}, {4, 10}};
    const auto build = [&](std::size_t index, std::uint32_t ticks)
    {
        return snake::sim::Simulation::from_setup({.snake = snake,
                                                   .food = {2, 2},
                                                   .seed = 1,
                                                   .ticks = ticks,
                                                   .variant = snake::sim::Variant::HOT_POTATO,
                                                   .lump = snake[index]});
    };
    const board_scene::Layout layout = board_scene::layout_for(QSize(480, 480));

    const auto far = build(2, 0);
    REQUIRE(far.has_value());
    const auto far_shapes = board_scene::game_shapes(layout, *far, 0.0);
    CHECK(colour_at(far_shapes, middle_of(layout, Cell{8, 10})) == opaque(tokens::INK)); // the ball, on its piece
    // Far from the tail the piece keeps its stripe: look at its edge.
    CHECK(colour_at(far_shapes, QPoint(layout.origin.x() + (8 * layout.cell) + 1,
                                       middle_of(layout, Cell{8, 10}).y())) == opaque(tokens::ACCENT));

    // Near the tail it flashes with the ticks.
    const auto near_even = build(5, 2);
    const auto near_odd = build(5, 3);
    REQUIRE(near_even.has_value());
    REQUIRE(near_odd.has_value());
    const QPoint edge(layout.origin.x() + (5 * layout.cell) + 2, middle_of(layout, Cell{5, 10}).y());
    CHECK(colour_at(board_scene::game_shapes(layout, *near_even, 0.0), edge) == opaque(tokens::TONGUE));
    CHECK(colour_at(board_scene::game_shapes(layout, *near_odd, 0.0), edge) != opaque(tokens::TONGUE));
}
