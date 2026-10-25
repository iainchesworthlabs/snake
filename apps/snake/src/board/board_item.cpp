// SPDX-License-Identifier: GPL-3.0-or-later
#include "board/board_item.hpp"

#include "board/board_scene.hpp"
#include "viewmodel/game_view_model.hpp"

#include <QMatrix4x4>

#include <QColor>
#include <QQuickWindow>
#include <QRect>
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

/// The children of the root node, drawn in this order.
enum class Layer : std::uint8_t
{
    BACKGROUND,
    GAME,
    EFFECTS,
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

/// The root moves the whole board, which is how it shakes.
QSGNode* make_root()
{
    auto* root = new QSGTransformNode;
    root->appendChildNode(new QSGNode);
    root->appendChildNode(new QSGNode);
    root->appendChildNode(new QSGNode);
    return root;
}

/// Makes the layer hold one rectangle node for each of the shapes, in order. The nodes come from the window, so the
/// board is drawn by every renderer, including the software one that the tests use.
void set_shapes(QQuickWindow& window, QSGNode& layer, const std::vector<board_scene::Shape>& shapes)
{
    auto have = static_cast<std::size_t>(layer.childCount());
    while (have < shapes.size())
    {
        layer.appendChildNode(window.createRectangleNode());
        ++have;
    }
    while (have > shapes.size())
    {
        QSGNode* last = layer.lastChild();
        layer.removeChildNode(last);
        delete last;
        --have;
    }
    QSGNode* node = layer.firstChild();
    for (const board_scene::Shape& shape : shapes)
    {
        auto* rectangle = static_cast<QSGRectangleNode*>(node);
        rectangle->setRect(shape.rect);
        rectangle->setColor(QColor::fromRgba(shape.colour));
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
        disconnect(m_Game, &GameViewModel::progressChanged, this, nullptr);
    }
    m_Game = game;
    if (m_Game)
    {
        connect(m_Game, &GameViewModel::boardChanged, this, [this] { update(); });
        connect(m_Game, &GameViewModel::progressChanged, this, [this] { update(); });
    }
    update();
    emit gameChanged();
}

void BoardItem::setEffects(EffectsModel* effects)
{
    if (effects == m_Effects)
    {
        return;
    }
    if (m_Effects)
    {
        disconnect(m_Effects, &EffectsModel::changed, this, nullptr);
    }
    m_Effects = effects;
    if (m_Effects)
    {
        connect(m_Effects, &EffectsModel::changed, this, [this] { update(); });
    }
    update();
    emit effectsChanged();
}

void BoardItem::setFoodLift(qreal lift)
{
    lift = std::clamp(lift, 0.0, 1.0);
    if (lift == m_FoodLift)
    {
        return;
    }
    m_FoodLift = lift;
    update();
    emit foodLiftChanged();
}

QSGNode* BoardItem::updatePaintNode(QSGNode* old_node, UpdatePaintNodeData* /*data*/)
{
    QSGNode* root = old_node != nullptr ? old_node : make_root();

    const board_scene::Layout layout = board_scene::layout_for(size().toSize());
    if (layout != m_DrawnLayout || old_node == nullptr)
    {
        set_shapes(*window(), layer_node(*root, Layer::BACKGROUND), board_scene::background_shapes(layout));
        m_DrawnLayout = layout;
    }
    const effects::DeathLook death = m_Effects ? m_Effects->effects().death_look() : effects::DeathLook{};
    set_shapes(*window(), layer_node(*root, Layer::GAME),
               m_Game ? board_scene::game_shapes(layout, m_Game->session().simulation(), m_FoodLift,
                                                 m_Game->tickProgress(), death)
                      : std::vector<board_scene::Shape>{});
    if (m_Effects)
    {
        set_shapes(*window(), layer_node(*root, Layer::EFFECTS),
                   board_scene::effect_shapes(layout, m_Effects->effects()));
        const QPoint shake = m_Effects->effects().shake() * layout.pixel();
        QMatrix4x4 matrix;
        matrix.translate(static_cast<float>(shake.x()), static_cast<float>(shake.y()));
        static_cast<QSGTransformNode*>(root)->setMatrix(matrix);
    }
    else
    {
        set_shapes(*window(), layer_node(*root, Layer::EFFECTS), {});
        static_cast<QSGTransformNode*>(root)->setMatrix(QMatrix4x4());
    }
    return root;
}

} // namespace snake::ui
