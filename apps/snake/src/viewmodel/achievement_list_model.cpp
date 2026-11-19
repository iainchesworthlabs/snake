// SPDX-License-Identifier: GPL-3.0-or-later
#include "viewmodel/achievement_list_model.hpp"

#include "snake/profile/achievements.hpp"
#include "snake/sim/scoring.hpp"

#include <QString>
#include <Qt>
#include <cstdint>

namespace snake::ui
{

namespace
{

constexpr int ROW_COUNT = profile::ACHIEVEMENT_COUNT;

} // namespace

AchievementListModel::AchievementListModel(const profile::Achievements& achievements, QObject* parent)
    : QAbstractListModel(parent), m_Achievements(achievements)
{
}

int AchievementListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ROW_COUNT;
}

QVariant AchievementListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= ROW_COUNT || index.column() != 0)
    {
        return {};
    }
    const auto achievement = static_cast<profile::Achievement>(index.row());
    switch (role)
    {
    case NumberRole:
        return index.row();
    case NameRole:
        return name_of(achievement);
    case DescriptionRole:
        return description_of(achievement);
    case EarnedRole:
        return m_Achievements.has(achievement);
    case ProgressRole:
        if (const auto progress = m_Achievements.progress(achievement))
        {
            return QStringLiteral("%L1 / %L2").arg(progress->current).arg(progress->target);
        }
        return QString();
    default:
        break;
    }
    return {};
}

QHash<int, QByteArray> AchievementListModel::roleNames() const
{
    return {
        {NumberRole, "number"},
        {NameRole, "name"},
        {DescriptionRole, "description"},
        {EarnedRole, "earned"},
        {ProgressRole, "progress"},
    };
}

void AchievementListModel::refresh()
{
    emit dataChanged(index(0), index(ROW_COUNT - 1));
}

QString AchievementListModel::name_of(profile::Achievement achievement)
{
    switch (achievement)
    {
    case profile::Achievement::WRAP:
        return tr("Through the wall");
    case profile::Achievement::BONUS:
        return tr("Bonus item");
    case profile::Achievement::COMBO:
        return tr("Top combo");
    case profile::Achievement::BEST:
        return tr("New best");
    case profile::Achievement::SPEED:
        return tr("Top speed");
    case profile::Achievement::BERRIES_100:
        return tr("%L1 blueberries").arg(profile::BERRIES_100_TARGET);
    case profile::Achievement::BERRIES_1000:
        return tr("%L1 blueberries").arg(profile::BERRIES_1000_TARGET);
    case profile::Achievement::PORTALS:
        return tr("Portals open");
    case profile::Achievement::PORTAL:
        return tr("Through a portal");
    case profile::Achievement::HOT_POTATO:
        return tr("Hot potato open");
    case profile::Achievement::SHED:
        return tr("Shed in time");
    case profile::Achievement::ALL_MODES:
        break;
    }
    return tr("All four modes");
}

QString AchievementListModel::description_of(profile::Achievement achievement)
{
    switch (achievement)
    {
    case profile::Achievement::WRAP:
        return tr("Leave one side of the board in Easy and come back on the other.");
    case profile::Achievement::BONUS:
        return tr("Eat a bonus item before it goes.");
    case profile::Achievement::COMBO:
        return tr("Eat a blueberry worth %L1 points.").arg(sim::Scoring::FOOD_POINTS * sim::Scoring::MAX_COMBO);
    case profile::Achievement::BEST:
        return tr("Beat the top score on a mode's leaderboard.");
    case profile::Achievement::SPEED:
        return tr("Eat until the snake is as fast as it gets.");
    case profile::Achievement::BERRIES_100:
        return tr("Eat %L1 blueberries, over as many games as it takes.").arg(profile::BERRIES_100_TARGET);
    case profile::Achievement::BERRIES_1000:
        return tr("Eat %L1 blueberries, over as many games as it takes.").arg(profile::BERRIES_1000_TARGET);
    case profile::Achievement::PORTALS:
        return tr("Open the Portals mode.");
    case profile::Achievement::PORTAL:
        return tr("Go into a portal and come out of its twin.");
    case profile::Achievement::HOT_POTATO:
        return tr("Open the Hot potato mode.");
    case profile::Achievement::SHED:
        return tr("Drop a bomb with Shed before it goes off.");
    case profile::Achievement::ALL_MODES:
        break;
    }
    return tr("Finish a game in each of the four modes.");
}

} // namespace snake::ui
