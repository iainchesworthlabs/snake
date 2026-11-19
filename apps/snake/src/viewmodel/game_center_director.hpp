// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/game_center.hpp"
#include "snake/profile/achievements.hpp"
#include "viewmodel/game_view_model.hpp"
#include "viewmodel/profile_view_model.hpp"

#include <QtQml/qqmlregistration.h>

#include <QObject>
#include <string_view>

namespace snake::ui
{

/// Game Center, for the player who is signed in to it: sends the score of each game when it ends and the achievements
/// the device holds, and opens Game Center's leaderboard for a mode and its achievements. It does nothing without a
/// Game Center that is signed in, which is every computer and a phone whose player has not signed in. The leaderboards
/// and achievements on the device are not touched either way: the device is the record and Game Center is a copy.
class GameCenterDirector : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(snake::ui::GameViewModel* game READ game WRITE setGame NOTIFY gameChanged FINAL)
    /// The profile that holds the achievements. What a game earns is reported when it ends, and everything the profile
    /// holds is reported whenever Game Center becomes available and whenever the profile is set or loads a save file.
    Q_PROPERTY(snake::ui::ProfileViewModel* profile READ profile WRITE setProfile NOTIFY profileChanged FINAL)
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

    [[nodiscard]] ProfileViewModel* profile() const noexcept
    {
        return m_Profile;
    }
    void setProfile(ProfileViewModel* profile);

    [[nodiscard]] bool available() const;

    /// Opens Game Center's leaderboard for `mode`.
    Q_INVOKABLE void showLeaderboard(snake::ui::GameViewModel::Mode mode);

    /// Opens Game Center's achievements.
    Q_INVOKABLE void showAchievements();

    /// The identifier of the leaderboard of a mode in App Store Connect. They are made there with exactly these
    /// identifiers (apps/snake/packaging/ios/game-center/components.json).
    [[nodiscard]] static std::string_view leaderboard_id(GameViewModel::Mode mode) noexcept;

    /// The identifier of an achievement in App Store Connect, `com.iainchesworthlabs.snake.achievement.<slug>`. Once
    /// an identifier is entered there it can never change, so none of these may.
    [[nodiscard]] static std::string_view achievement_id(profile::Achievement achievement) noexcept;

signals:
    void gameChanged();
    void profileChanged();
    void availableChanged();

private:
    void on_state_changed();
    void on_availability_changed();
    /// Reports what the game that just ended earned, and how far each count that is not earned has come.
    void report_result();
    /// Reports everything the profile holds.
    void report_held();
    /// Reports each achievement in `earned` at 100 and each count the profile has not earned at its percentage, when
    /// that is above zero.
    void report(const profile::Achievements& earned);

    device::GameCenter* m_GameCenter = nullptr;
    GameViewModel* m_Game = nullptr;
    ProfileViewModel* m_Profile = nullptr;
};

} // namespace snake::ui
