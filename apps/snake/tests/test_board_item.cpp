// SPDX-License-Identifier: GPL-3.0-or-later
#include "board/board_item.hpp"
#include "snake/sim/autoplayer.hpp"
#include "snake/sim/cell.hpp"
#include "snake/sim/direction.hpp"
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
namespace colours = snake::ui::board_colors;

namespace
{

constexpr QRgb WINDOW_COLOUR = 0xFF000000;
constexpr int CELL = 20; // pixels, in a window 400 pixels square

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

/// The colour in the middle of a cell of a board that fills a window 400 pixels square.
QRgb colour_of(const QImage& image, Cell cell)
{
    return image.pixel((cell.x * CELL) + (CELL / 2), (cell.y * CELL) + (CELL / 2));
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

TEST_CASE("A board with no game draws nothing", "[board]")
{
    Scene scene;
    const QImage image = scene.render();
    REQUIRE(image.width() == 400);
    CHECK(colour_of(image, Cell{0, 0}) == WINDOW_COLOUR);
    CHECK(colour_of(image, Cell{10, 10}) == WINDOW_COLOUR);
}

TEST_CASE("A new game shows the snake, the food and the board", "[board]")
{
    GameViewModel model(counting_seeds());
    Scene scene;
    scene.board().setGame(&model);
    const QImage image = scene.render();

    // The snake starts at the middle of the board heading right, with its body trailing to the left.
    CHECK(colour_of(image, Cell{10, 10}) == colours::HEAD);
    CHECK(colour_of(image, Cell{9, 10}) == colours::BODY);
    CHECK(colour_of(image, Cell{8, 10}) == colours::BODY);
    CHECK(colour_of(image, model.session().simulation().food()) == colours::FOOD);
    CHECK(colour_of(image, empty_cell(model)) == colours::BACKGROUND);
}

TEST_CASE("The board is the largest square that fits, in the middle", "[board]")
{
    GameViewModel model(counting_seeds());
    Scene scene(QSize(400, 200));
    scene.board().setGame(&model);
    const QImage image = scene.render();

    // 200 pixels square, from x = 100: cells of 10 pixels.
    CHECK(image.pixel(50, 100) == WINDOW_COLOUR);
    CHECK(image.pixel(350, 100) == WINDOW_COLOUR);
    CHECK(image.pixel(100 + (10 * 10) + 5, (10 * 10) + 5) == colours::HEAD);
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
    CHECK(colour_of(image, Cell{11, 10}) == colours::HEAD);
    CHECK(colour_of(image, Cell{10, 10}) == colours::BODY);
    CHECK(colour_of(image, Cell{9, 10}) == colours::BODY);
    // The tail has moved on, so its cell is empty again (unless the food is there).
    if (model.session().simulation().food() != Cell{8, 10})
    {
        CHECK(colour_of(image, Cell{8, 10}) == colours::BACKGROUND);
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
    CHECK(colour_of(image, model.session().simulation().bonus()->cell) == colours::BONUS);
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
    CHECK(colour_of(scene.render(), Cell{10, 10}) == colours::HEAD);

    scene.board().setGame(&second);
    CHECK(changes == 2);
    // The old game's signals no longer reach the board.
    first->newGame();
    CHECK(colour_of(scene.render(), second.session().simulation().food()) == colours::FOOD);

    first.reset();
    scene.board().setGame(nullptr);
    CHECK(changes == 3);
    CHECK(colour_of(scene.render(), Cell{10, 10}) == WINDOW_COLOUR);
}

TEST_CASE("A game that is deleted while the board shows it leaves an empty board", "[board]")
{
    auto model = std::make_unique<GameViewModel>(counting_seeds());
    Scene scene;
    scene.board().setGame(model.get());
    REQUIRE(colour_of(scene.render(), Cell{10, 10}) == colours::HEAD);
    model.reset();
    scene.board().update();
    CHECK(scene.board().game() == nullptr);
    CHECK(colour_of(scene.render(), Cell{10, 10}) == WINDOW_COLOUR);
}
