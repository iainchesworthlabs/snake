// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "platform/save_file.hpp"
#include "snake/profile/player_name.hpp"
#include "snake/profile/save_data.hpp"
#include "snake/sim/mode.hpp"
#include "viewmodel/game_view_model.hpp"
#include "viewmodel/leaderboard_model.hpp"
#include "viewmodel/mode_list_model.hpp"

#include <QtQml/qqmlregistration.h>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace snake::ui
{

/// What the player keeps and the screens show of it: the list of modes with the ones still locked, the best score of
/// the mode being played, and what the last game came to. It watches a GameViewModel and notes each game that ends.
/// Without a save file the profile lives in memory. Once a file is set the profile loads it and writes it after every
/// change, so the leaderboards, the recent names and the unlocks last between runs. A game that makes the top 10
/// waits for its name (see the name entry properties) and goes on the leaderboard when it is saved.
class ProfileViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    /// The game to watch. Set once, when the profile is created.
    Q_PROPERTY(snake::ui::GameViewModel* game READ game WRITE setGame NOTIFY gameChanged FINAL)
    Q_PROPERTY(snake::ui::ModeListModel* modes READ modes CONSTANT FINAL)
    /// The best score of the mode of the current game, as six digits.
    Q_PROPERTY(QString best READ best NOTIFY bestChanged FINAL)
    /// The last game beat the best score its mode had before it.
    Q_PROPERTY(bool newBest READ newBest NOTIFY resultChanged FINAL)
    /// How far the last game was from the best score of its mode: 0 when it matched it or beat it, and -1 when the
    /// mode had no best score before and the game scored nothing.
    Q_PROPERTY(int pointsToBest READ pointsToBest NOTIFY resultChanged FINAL)
    /// The names of the modes the last game opened, in the order of the list. Empty when it opened none.
    Q_PROPERTY(QStringList unlockedNow READ unlockedNow NOTIFY resultChanged FINAL)
    /// The file the profile is kept in, or empty for none. Setting it loads the file when there is one.
    Q_PROPERTY(QString saveFile READ saveFile WRITE setSaveFile NOTIFY saveFileChanged FINAL)
    /// True while changes are being written to the save file: false without a file, and after the file was found to
    /// come from a newer program, which this one must not overwrite.
    Q_PROPERTY(bool persistent READ persistent NOTIFY saveFileChanged FINAL)

    /// The leaderboard the screen shows: its lines, and the mode they are of.
    Q_PROPERTY(snake::ui::LeaderboardModel* leaderboard READ leaderboard CONSTANT FINAL)
    Q_PROPERTY(snake::ui::GameViewModel::Mode boardMode READ boardMode WRITE setBoardMode NOTIFY boardModeChanged FINAL)

    /// How the player steers on a touch device. Keyboard and gamepad always work.
    Q_PROPERTY(Controls controls READ controls WRITE setControls NOTIFY settingsChanged FINAL)
    /// Tilt and the touch pad steer the other way round.
    Q_PROPERTY(bool invertAxes READ invertAxes WRITE setInvertAxes NOTIFY settingsChanged FINAL)
    Q_PROPERTY(bool sound READ sound WRITE setSound NOTIFY settingsChanged FINAL)
    Q_PROPERTY(bool music READ music WRITE setMusic NOTIFY settingsChanged FINAL)
    /// Turns off the motion that is not needed to play: shake, flashing and the bobbing of the food.
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY settingsChanged FINAL)

    /// The last game made the top 10 of its mode and waits for a name.
    Q_PROPERTY(bool qualifies READ qualifies NOTIFY pendingChanged FINAL)
    /// The score that waits for a name, as six digits. Empty when none waits.
    Q_PROPERTY(QString pendingScore READ pendingScore NOTIFY pendingChanged FINAL)
    /// The place the waiting score will take, counting from 1. 0 when none waits.
    Q_PROPERTY(int pendingRank READ pendingRank NOTIFY pendingChanged FINAL)
    /// The name being typed, in capitals.
    Q_PROPERTY(QString entryName READ entryName NOTIFY entryChanged FINAL)
    /// The name has a letter, so it can be saved.
    Q_PROPERTY(bool canSave READ canSave NOTIFY entryChanged FINAL)
    /// The longest name there can be.
    Q_PROPERTY(int nameLength READ nameLength CONSTANT FINAL)
    /// The names used last, newest first: at most three.
    Q_PROPERTY(QStringList recentNames READ recentNames NOTIFY recentNamesChanged FINAL)
    /// Up to three lines of the leaderboard as it will be with the waiting score in: the entry above it, the score with
    /// the name typed so far, and the entry it pushes down. Each is a map of place, name, score (six digits) and mine.
    Q_PROPERTY(QVariantList standing READ standing NOTIFY standingChanged FINAL)

public:
    /// The time of the end of a game, in seconds since 1 January 1970 UTC.
    using Clock = std::function<std::int64_t()>;
    /// Says whether the system asks for less motion.
    using MotionPreference = std::function<bool()>;

    /// How the player steers on a touch device. The values are those of profile::ControlScheme.
    enum class Controls : std::uint8_t
    {
        Tilt,
        Swipe,
        Dpad,
    };
    Q_ENUM(Controls)

    /// The clock is the system's.
    explicit ProfileViewModel(QObject* parent = nullptr);
    /// Tests give the time.
    explicit ProfileViewModel(Clock clock, QObject* parent = nullptr);
    /// Tests give the time and what the system asks for. On a first run, with a save file set, the reduced-motion
    /// setting starts on when the system asks for less motion.
    ProfileViewModel(Clock clock, MotionPreference motion_preference, QObject* parent = nullptr);

    [[nodiscard]] GameViewModel* game() const noexcept
    {
        return m_Game;
    }
    void setGame(GameViewModel* game);

    [[nodiscard]] ModeListModel* modes() noexcept
    {
        return &m_Modes;
    }

    [[nodiscard]] LeaderboardModel* leaderboard() noexcept
    {
        return &m_Board;
    }
    [[nodiscard]] GameViewModel::Mode boardMode() const noexcept
    {
        return m_BoardMode;
    }
    void setBoardMode(GameViewModel::Mode mode);

    [[nodiscard]] QString best() const;
    [[nodiscard]] bool newBest() const noexcept
    {
        return m_NewBest;
    }
    [[nodiscard]] int pointsToBest() const noexcept
    {
        return m_PointsToBest;
    }
    [[nodiscard]] QStringList unlockedNow() const
    {
        return m_UnlockedNow;
    }

    [[nodiscard]] QString saveFile() const;
    void setSaveFile(const QString& path);
    [[nodiscard]] bool persistent() const noexcept
    {
        return m_Writing;
    }

    [[nodiscard]] bool qualifies() const noexcept
    {
        return m_Pending.has_value();
    }
    [[nodiscard]] QString pendingScore() const;
    [[nodiscard]] int pendingRank() const noexcept
    {
        return m_Pending ? static_cast<int>(m_Pending->place) + 1 : 0;
    }
    [[nodiscard]] QString entryName() const;
    [[nodiscard]] bool canSave() const noexcept
    {
        return m_Pending.has_value() && m_Name.can_save();
    }
    [[nodiscard]] static int nameLength() noexcept
    {
        return static_cast<int>(profile::PlayerName::MAX_LENGTH);
    }
    [[nodiscard]] QStringList recentNames() const;
    [[nodiscard]] QVariantList standing() const;

    [[nodiscard]] Controls controls() const noexcept;
    void setControls(Controls controls);
    [[nodiscard]] bool invertAxes() const noexcept
    {
        return m_Data.settings.invert_axes;
    }
    void setInvertAxes(bool invert);
    [[nodiscard]] bool sound() const noexcept
    {
        return m_Data.settings.sound;
    }
    void setSound(bool on);
    [[nodiscard]] bool music() const noexcept
    {
        return m_Data.settings.music;
    }
    void setMusic(bool on);
    [[nodiscard]] bool reducedMotion() const noexcept
    {
        return m_Data.settings.reduced_motion;
    }
    void setReducedMotion(bool reduced);

    /// Empties the leaderboard of `mode`. The modes it opened stay open.
    Q_INVOKABLE void resetLeaderboard(snake::ui::GameViewModel::Mode mode);
    /// The player asked for the device's position to become the neutral one. The tilt source answers the signal.
    Q_INVOKABLE void calibrateTilt();

    /// Adds the letter or the space in `character` to the end of the name. Anything else, and a letter that does not
    /// fit, is ignored.
    Q_INVOKABLE void typeCharacter(const QString& character);
    /// Takes the last letter off the name.
    Q_INVOKABLE void eraseCharacter();
    /// Puts the recent name in place `index` (0 is the newest) in the place of the name typed.
    Q_INVOKABLE void chooseRecent(int index);
    /// Puts the waiting score on the leaderboard under the name and remembers the name. Returns false, changing
    /// nothing, when no score waits or the name has no letter.
    Q_INVOKABLE bool saveScore();
    /// Lets the waiting score go.
    Q_INVOKABLE void discardScore();

    [[nodiscard]] const profile::SaveData& data() const noexcept
    {
        return m_Data;
    }

    /// Notes a game that ended with `score` in `mode`: the best score, the unlocks and the result the screens show.
    /// The view-model calls it for each game it watches; the tests call it for scores that are hard to play.
    void record_game(sim::Mode mode, std::uint32_t score);

signals:
    void gameChanged();
    void bestChanged();
    void saveFileChanged();
    /// Any of the settings changed, or they were loaded.
    void settingsChanged();
    /// The player asked to recalibrate tilt.
    void calibrationRequested();
    void boardModeChanged();
    /// A score began to wait for a name, or stopped.
    void pendingChanged();
    void entryChanged();
    void recentNamesChanged();
    void standingChanged();
    /// A game ended and its result is in newBest, pointsToBest and unlockedNow.
    void resultChanged();

private:
    /// A game that made the top 10 and has not been saved.
    struct Pending
    {
        sim::Mode mode = sim::Mode::EASY;
        std::uint32_t score = 0;
        std::int64_t unix_seconds = 0;
        std::size_t place = 0;
    };

    void on_state_changed();
    /// The best score of `mode`: the leaderboard's, or the waiting score when that is higher.
    [[nodiscard]] std::uint32_t best_of(sim::Mode mode) const noexcept;
    void write();
    /// Changes the settings through `change`, then writes them and tells QML when something is different.
    template <typename Change>
    void change_settings(Change change);
    void set_pending(const std::optional<Pending>& pending);

    GameViewModel* m_Game = nullptr;
    Clock m_Clock;
    MotionPreference m_MotionPreference;
    profile::SaveData m_Data;
    ModeListModel m_Modes;
    LeaderboardModel m_Board;
    GameViewModel::Mode m_BoardMode = GameViewModel::Mode::Easy;
    std::unique_ptr<SaveFile> m_File;
    bool m_Writing = false;
    std::optional<Pending> m_Pending;
    profile::PlayerName m_Name;
    bool m_NewBest = false;
    int m_PointsToBest = -1;
    QStringList m_UnlockedNow;
};

} // namespace snake::ui
