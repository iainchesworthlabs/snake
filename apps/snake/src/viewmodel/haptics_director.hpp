// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/haptics.hpp"
#include "viewmodel/game_view_model.hpp"

#include <QtQml/qqmlregistration.h>

#include <QObject>

namespace snake::ui
{

/// What the hand feels: a light tap when the snake eats, a knock when it crashes. It watches the game's events and
/// tells a Haptics, and does nothing without one, which is every computer.
class HapticsDirector : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(snake::ui::GameViewModel* game READ game WRITE setGame NOTIFY gameChanged FINAL)

public:
    /// Uses the program's haptics (see device::default_haptics()).
    explicit HapticsDirector(QObject* parent = nullptr);
    /// Uses `haptics`, which must outlive the director. Tests give a recording one.
    explicit HapticsDirector(device::Haptics* haptics, QObject* parent = nullptr);

    [[nodiscard]] GameViewModel* game() const noexcept
    {
        return m_Game;
    }
    void setGame(GameViewModel* game);

signals:
    void gameChanged();

private:
    void on_tick_event(GameViewModel::Event event);

    device::Haptics* m_Haptics = nullptr;
    GameViewModel* m_Game = nullptr;
};

} // namespace snake::ui
