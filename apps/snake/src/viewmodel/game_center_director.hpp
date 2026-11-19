// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/game_center.hpp"
#include "viewmodel/game_view_model.hpp"

#include <QtQml/qqmlregistration.h>

#include <QObject>
#include <string_view>

namespace snake::ui
{

/// Game Center, for the player who is signed in to it: sends the score of each game when it ends, and opens Game
/// Center's leaderboard for a mode. It does nothing without a Game Center that is signed in, which is every computer
/// and a phone whose player has not signed in. The leaderboards on the device are not touched either way.
class GameCenterDirector : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(snake::ui::GameViewModel* game READ game WRITE setGame NOTIFY gameChanged FINAL)
    /// Whether the player is signed in to Game Center now: whether the button for it is shown.
    Q_PROPERTY(bool available READ available NOTIFY availableChanged FINAL)

public:
    /// Uses the program's Game Center (see device::default_game_center()).
    explicit GameCenterDirector(QObject* parent = nullptr);
    /// Uses `game_center`, which must outlive the director. Tests give a recording one.
    explicit GameCenterDirector(device::GameCenter* game_center, QObject* parent = nullptr);
    ~GameCenterDirector() override;

    [[nodiscard]] GameViewModel* game() const noexcept
    {
        return m_Game;
    }
    void setGame(GameViewModel* game);

    [[nodiscard]] bool available() const;

    /// Opens Game Center's leaderboard for `mode`.
    Q_INVOKABLE void showLeaderboard(snake::ui::GameViewModel::Mode mode);

    /// The identifier of the leaderboard of a mode in App Store Connect. They are made there with exactly these
    /// identifiers (docs/app-store.md).
    [[nodiscard]] static std::string_view leaderboard_id(GameViewModel::Mode mode) noexcept;

signals:
    void gameChanged();
    void availableChanged();

private:
    void on_state_changed();

    device::GameCenter* m_GameCenter = nullptr;
    GameViewModel* m_Game = nullptr;
};

} // namespace snake::ui
