// SPDX-License-Identifier: GPL-3.0-or-later
#include "viewmodel/leaderboard_model.hpp"

#include "snake/profile/leaderboard.hpp"
#include "snake/sim/mode.hpp"
#include "snake/sim/scoring.hpp"

#include <QByteArray>
#include <QHash>
#include <QModelIndex>
#include <QString>
#include <QVariant>
#include <Qt>
#include <cstddef>
#include <optional>

namespace snake::ui
{

LeaderboardModel::LeaderboardModel(const profile::Leaderboards& boards, QObject* parent)
    : QAbstractListModel(parent), m_Boards(boards)
{
}

int LeaderboardModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_Boards.of(m_Mode).size());
}

QVariant LeaderboardModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount())
    {
        return {};
    }
    const auto place = static_cast<std::size_t>(index.row());
    const profile::LeaderboardEntry& entry = m_Boards.of(m_Mode).at(place);
    switch (role)
    {
    case PlaceRole:
        return index.row() + 1;
    case NameRole:
        return QString::fromLatin1(entry.name.view().data(), static_cast<qsizetype>(entry.name.view().size()));
    case ScoreRole:
    {
        const auto digits = sim::score_digits(entry.score);
        return QString::fromLatin1(digits.data(), static_cast<qsizetype>(digits.size()));
    }
    case LatestRole:
        return m_LatestMode == m_Mode && m_LatestPlace == place;
    default:
        return {};
    }
}

QHash<int, QByteArray> LeaderboardModel::roleNames() const
{
    return {{PlaceRole, "place"}, {NameRole, "name"}, {ScoreRole, "score"}, {LatestRole, "latest"}};
}

void LeaderboardModel::show(sim::Mode mode)
{
    beginResetModel();
    m_Mode = mode;
    endResetModel();
}

void LeaderboardModel::mark_latest(std::optional<sim::Mode> mode, std::size_t place)
{
    beginResetModel();
    m_LatestMode = mode;
    m_LatestPlace = place;
    endResetModel();
}

void LeaderboardModel::refresh()
{
    beginResetModel();
    endResetModel();
}

} // namespace snake::ui
