// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "board/board_scene.hpp"
#include "viewmodel/game_view_model.hpp"

#include <QtQml/qqmlregistration.h>

#include <QPointer>
#include <QQuickItem>

class QSGNode;

namespace snake::ui
{

/// Draws the board of a GameViewModel in the Toybox Pixel style: the framed checkerboard, the striped snake with its
/// head, eyes and tongue, the food and the bonus item. board_scene works out the rectangles and this item hands them
/// to the scene graph. The board redraws when the view-model says it changed, and it holds no rules and no state of
/// its own beyond how far the food is raised.
class BoardItem : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(snake::ui::GameViewModel* game READ game WRITE setGame NOTIFY gameChanged FINAL)
    /// How far the food and the bonus item are raised, from 0 to 1. A QML animation moves it up and down so the food
    /// bobs.
    Q_PROPERTY(qreal foodLift READ foodLift WRITE setFoodLift NOTIFY foodLiftChanged FINAL)

public:
    explicit BoardItem(QQuickItem* parent = nullptr);

    [[nodiscard]] GameViewModel* game() const
    {
        return m_Game;
    }

    void setGame(GameViewModel* game);

    [[nodiscard]] qreal foodLift() const
    {
        return m_FoodLift;
    }

    void setFoodLift(qreal lift);

signals:
    void gameChanged();
    void foodLiftChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* old_node, UpdatePaintNodeData* data) override;

private:
    QPointer<GameViewModel> m_Game;
    qreal m_FoodLift = 0.0;
    /// The layout the background was last drawn for, so that it is drawn again only when the layout changes.
    board_scene::Layout m_DrawnLayout;
};

} // namespace snake::ui
