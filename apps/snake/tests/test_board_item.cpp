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
