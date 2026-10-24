// SPDX-License-Identifier: GPL-3.0-or-later
#include "board/board_item.hpp"

#include "snake/sim/cell.hpp"
#include "snake/sim/simulation.hpp"
#include "viewmodel/game_view_model.hpp"

#include <QColor>
#include <QQuickWindow>
#include <QRectF>
#include <QSGNode>
#include <QSGRectangleNode>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace snake::ui
{

namespace
{

/// The space between two cells, as a fraction of a cell, so that the segments of the snake can be told apart.
constexpr qreal CELL_GAP = 0.06;

/// The children of the root node, drawn in this order. Each is a container for the rectangles of one colour.
enum class Layer : std::uint8_t
{
    BOARD,
    BODY,
    HEAD,
    FOOD,
    BONUS,
};

QSGNode& layer_node(QSGNode& root, Layer layer)
{
    QSGNode* node = root.firstChild();
    for (auto index = static_cast<std::size_t>(layer); index > 0; --index)
    {
        node = node->nextSibling();
    }
    return *node;
}

QSGNode* make_root()
{
    auto* root = new QSGNode;
    for (int layer = 0; layer <= static_cast<int>(Layer::BONUS); ++layer)
    {
        root->appendChildNode(new QSGNode);
    }
    return root;
}

/// Makes the layer hold one rectangle node for each of `rects`, in `colour`. The nodes come from the window, so the
/// board is drawn by every renderer, including the software one that the tests use.
void set_rects(QQuickWindow& window, QSGNode& layer, const std::vector<QRectF>& rects, QRgb colour)
{
    auto have = static_cast<std::size_t>(layer.childCount());
    while (have < rects.size())
    {
        layer.appendChildNode(window.createRectangleNode());
        ++have;
    }
    while (have > rects.size())
    {
        QSGNode* last = layer.lastChild();
        layer.removeChildNode(last);
        delete last;
        --have;
    }
    QSGNode* node = layer.firstChild();
    for (const QRectF& rect : rects)
    {
        auto* rectangle = static_cast<QSGRectangleNode*>(node);
        rectangle->setRect(rect);
        rectangle->setColor(QColor::fromRgba(colour));
        node = node->nextSibling();
    }
}

} // namespace

BoardItem::BoardItem(QQuickItem* parent) : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

void BoardItem::setGame(GameViewModel* game)
{
    if (game == m_Game)
    {
        return;
    }
    if (m_Game)
    {
        disconnect(m_Game, &GameViewModel::boardChanged, this, nullptr);
    }
    m_Game = game;
    if (m_Game)
    {
        connect(m_Game, &GameViewModel::boardChanged, this, [this] { update(); });
    }
    update();
    emit gameChanged();
}

QSGNode* BoardItem::updatePaintNode(QSGNode* old_node, UpdatePaintNodeData* /*data*/)
{
    QSGNode* root = old_node != nullptr ? old_node : make_root();

    const qreal side = std::min(width(), height());
    const qreal left = (width() - side) / 2.0;
    const qreal top = (height() - side) / 2.0;
    const qreal cell = side / sim::BOARD_COLUMNS;
    const auto rect_of = [&](sim::Cell at)
    {
        return QRectF(left + (at.x * cell) + (CELL_GAP * cell), top + (at.y * cell) + (CELL_GAP * cell),
                      cell * (1.0 - (2.0 * CELL_GAP)), cell * (1.0 - (2.0 * CELL_GAP)));
    };

    std::vector<QRectF> board;
    std::vector<QRectF> body;
    std::vector<QRectF> head;
    std::vector<QRectF> food;
    std::vector<QRectF> bonus;
    if (m_Game && side > 0.0)
    {
        board.emplace_back(left, top, side, side);
        const sim::Simulation& game = m_Game->session().simulation();
        const auto& snake = game.snake();
        head.push_back(rect_of(game.head()));
        body.reserve(snake.size());
        for (std::size_t index = 1; index < snake.size(); ++index)
        {
            body.push_back(rect_of(snake[index]));
        }
        // After a win there is no room for the food.
        if (!game.won())
        {
            food.push_back(rect_of(game.food()));
        }
        if (game.bonus())
        {
            bonus.push_back(rect_of(game.bonus()->cell));
        }
    }
    set_rects(*window(), layer_node(*root, Layer::BOARD), board, board_colors::BACKGROUND);
    set_rects(*window(), layer_node(*root, Layer::BODY), body, board_colors::BODY);
    set_rects(*window(), layer_node(*root, Layer::HEAD), head, board_colors::HEAD);
    set_rects(*window(), layer_node(*root, Layer::FOOD), food, board_colors::FOOD);
    set_rects(*window(), layer_node(*root, Layer::BONUS), bonus, board_colors::BONUS);
    return root;
}

} // namespace snake::ui
