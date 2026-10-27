// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "viewmodel/game_view_model.hpp"

#include <QtQml/qqmlregistration.h>

#include <QObject>
#include <functional>

namespace snake::ui
{

/// Keeps the screen of a phone from turning while a game is on: tilting the phone to steer would otherwise turn it
/// over. The orientation is held from the start of a game until the player is back at the title, which is what the PRD
/// means by locking it for the session, and the menus turn with the phone.
class OrientationDirector : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(snake::ui::GameViewModel* game READ game WRITE setGame NOTIFY gameChanged FINAL)
    /// Whether this is a device whose screen turns. A computer's window does not, so there is nothing to hold.
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged FINAL)
    /// Whether the orientation is held now.
    Q_PROPERTY(bool locked READ locked NOTIFY lockedChanged FINAL)

public:
    using Lock = std::function<void(bool)>;

    /// Holds and releases the orientation of this device (device::lock_orientation()).
    explicit OrientationDirector(QObject* parent = nullptr);
    /// Calls `lock` instead. Tests give a recording one.
    explicit OrientationDirector(Lock lock, QObject* parent = nullptr);
    ~OrientationDirector() override;

    [[nodiscard]] GameViewModel* game() const noexcept
    {
        return m_Game;
    }
    void setGame(GameViewModel* game);

    [[nodiscard]] bool active() const noexcept
    {
        return m_Active;
    }
    void setActive(bool active);

    [[nodiscard]] bool locked() const noexcept
    {
        return m_Locked;
    }

signals:
    void gameChanged();
    void activeChanged();
    void lockedChanged();

private:
    void update();

    Lock m_Lock;
    GameViewModel* m_Game = nullptr;
    bool m_Active = false;
    bool m_Locked = false;
};

} // namespace snake::ui
