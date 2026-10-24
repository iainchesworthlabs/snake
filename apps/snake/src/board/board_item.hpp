// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "viewmodel/game_view_model.hpp"

#include <QtQml/qqmlregistration.h>

#include <QPointer>
#include <QQuickItem>
#include <QRgb>

class QSGNode;

namespace snake::ui
{

/// The colours of the first board. They are placeholders until the theme tokens arrive with the board art.
namespace board_colors
{
inline constexpr QRgb BACKGROUND = 0xFF1B2A38;
inline constexpr QRgb BODY = 0xFF4CAF50;
inline constexpr QRgb HEAD = 0xFFA5D66A;
inline constexpr QRgb FOOD = 0xFFEF5350;
inline constexpr QRgb BONUS = 0xFFFFD54F;
} // namespace board_colors

/// Draws the board of a GameViewModel as plain rectangles: the board, the body, the head, the food and the bonus item.
/// The board is the largest square that fits the item, centred in it, and it redraws when the view-model says the
/// board changed. It holds no rules and no state of its own.
class BoardItem : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(snake::ui::GameViewModel* game READ game WRITE setGame NOTIFY gameChanged FINAL)

public:
    explicit BoardItem(QQuickItem* parent = nullptr);

    [[nodiscard]] GameViewModel* game() const
    {
        return m_Game;
    }

    void setGame(GameViewModel* game);

signals:
    void gameChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* old_node, UpdatePaintNodeData* data) override;

private:
    QPointer<GameViewModel> m_Game;
};

} // namespace snake::ui
