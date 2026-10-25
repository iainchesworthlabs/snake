// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/profile/save_data.hpp"
#include "snake/sim/mode.hpp"
#include "viewmodel/game_view_model.hpp"
#include "viewmodel/mode_list_model.hpp"

#include <QtQml/qqmlregistration.h>

#include <QObject>
#include <QString>
#include <QStringList>
#include <array>
#include <cstdint>

namespace snake::ui
{

/// What the player keeps and the screens show of it: the list of modes with the ones still locked, the best score of
/// the mode being played, and what the last game came to. It watches a GameViewModel and notes each game that ends.
/// The profile lives in memory for now; the platform service that reads and writes the save file comes later and
/// fills the same data.
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

public:
    explicit ProfileViewModel(QObject* parent = nullptr);

    [[nodiscard]] GameViewModel* game() const noexcept
    {
        return m_Game;
    }
    void setGame(GameViewModel* game);

    [[nodiscard]] ModeListModel* modes() noexcept
    {
        return &m_Modes;
    }

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
    /// A game ended and its result is in newBest, pointsToBest and unlockedNow.
    void resultChanged();

private:
    void on_state_changed();

    GameViewModel* m_Game = nullptr;
    profile::SaveData m_Data;
    ModeListModel m_Modes;
    /// Best scores of this run, by mode, until the leaderboards are kept (they need a name for each entry).
    std::array<std::uint32_t, profile::MODE_COUNT> m_Best{};
    bool m_NewBest = false;
    int m_PointsToBest = -1;
    QStringList m_UnlockedNow;
};

} // namespace snake::ui
