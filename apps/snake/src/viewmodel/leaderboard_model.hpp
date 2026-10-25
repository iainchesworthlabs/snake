// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/profile/leaderboard.hpp"
#include "snake/sim/mode.hpp"

#include <QtQml/qqmlregistration.h>

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QModelIndex>
#include <QVariant>
#include <cstddef>
#include <optional>

namespace snake::ui
{

/// The lines of the leaderboard of one mode, best first: at most ten. The mode shown can be changed, and the line
/// of the score saved last is marked so that the screen can highlight it.
class LeaderboardModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The profile owns the leaderboard.")

public:
    enum Role
    {
        /// The place, counting from 1.
        PlaceRole = Qt::UserRole + 1,
        NameRole,
        /// The score as six digits.
        ScoreRole,
        /// True for the line of the score saved last.
        LatestRole,
    };
    Q_ENUM(Role)

    /// `boards` must outlive the model. Call refresh() when it changes.
    explicit LeaderboardModel(const profile::Leaderboards& boards, QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    /// Shows the leaderboard of `mode`.
    void show(sim::Mode mode);
    /// Marks the line in place `place` (0 is the top) of the board of `mode`, or no line.
    void mark_latest(std::optional<sim::Mode> mode, std::size_t place);
    /// The boards changed: every line is read again.
    void refresh();

private:
    const profile::Leaderboards& m_Boards;
    sim::Mode m_Mode = sim::Mode::EASY;
    std::optional<sim::Mode> m_LatestMode;
    std::size_t m_LatestPlace = 0;
};

} // namespace snake::ui
