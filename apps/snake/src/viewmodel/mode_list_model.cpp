// SPDX-License-Identifier: GPL-3.0-or-later
#include "viewmodel/mode_list_model.hpp"

#include "snake/profile/unlocks.hpp"
#include "snake/sim/mode.hpp"
#include "viewmodel/game_view_model.hpp"

#include <QString>
#include <Qt>
#include <optional>

namespace snake::ui
{

namespace
{

constexpr int ENTRY_COUNT = 4;

constexpr std::optional<profile::Unlockable> unlockable_of(ModeListModel::Entry entry)
{
    switch (entry)
    {
    case ModeListModel::Entry::Portals:
        return profile::Unlockable::PORTALS;
    case ModeListModel::Entry::HotPotato:
        return profile::Unlockable::HOT_POTATO;
    case ModeListModel::Entry::Easy:
    case ModeListModel::Entry::Classic:
        break;
    }
    return std::nullopt;
}

constexpr ModeListModel::Entry entry_of(sim::Mode mode)
{
    return mode == sim::Mode::EASY ? ModeListModel::Entry::Easy : ModeListModel::Entry::Classic;
}

constexpr int game_mode_of(ModeListModel::Entry entry)
{
    switch (entry)
    {
    case ModeListModel::Entry::Easy:
        return static_cast<int>(GameViewModel::Mode::Easy);
    case ModeListModel::Entry::Classic:
        return static_cast<int>(GameViewModel::Mode::Classic);
    case ModeListModel::Entry::Portals:
    case ModeListModel::Entry::HotPotato:
        break;
    }
    return -1;
}

} // namespace

ModeListModel::ModeListModel(const profile::Unlocks& unlocks, QObject* parent)
    : QAbstractListModel(parent), m_Unlocks(unlocks)
{
}

int ModeListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ENTRY_COUNT;
}

QVariant ModeListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= ENTRY_COUNT || index.column() != 0)
    {
        return {};
    }
    const auto entry = static_cast<Entry>(index.row());
    switch (role)
    {
    case EntryRole:
        return static_cast<int>(entry);
    case NameRole:
        return name_of(entry);
    case DescriptionRole:
        switch (entry)
        {
        case Entry::Easy:
            return tr("Slower snake. Walls wrap around.");
        case Entry::Classic:
            return tr("Faster snake. Walls end the game.");
        case Entry::Portals:
        case Entry::HotPotato:
            break;
        }
        return tr("Coming in a later version.");
    case LockedRole:
        return locked(entry);
    case SelectableRole:
        return selectable(entry);
    case DetailRole:
        if (const auto unlockable = unlockable_of(entry); unlockable && locked(entry))
        {
            const profile::UnlockRule rule = profile::unlock_rule(*unlockable);
            return tr("Unlocks at %L1 in %2").arg(rule.score).arg(name_of(entry_of(rule.mode)));
        }
        return data(index, DescriptionRole);
    case GameModeRole:
        return game_mode_of(entry);
    default:
        break;
    }
    return {};
}

QHash<int, QByteArray> ModeListModel::roleNames() const
{
    return {
        {EntryRole, "entry"},
        {NameRole, "name"},
        {DescriptionRole, "description"},
        {LockedRole, "locked"},
        {SelectableRole, "selectable"},
        {DetailRole, "detail"},
        {GameModeRole, "gameMode"},
    };
}

void ModeListModel::refresh()
{
    emit dataChanged(index(0), index(ENTRY_COUNT - 1));
}

QString ModeListModel::name_of(Entry entry)
{
    switch (entry)
    {
    case Entry::Easy:
        return tr("Easy");
    case Entry::Classic:
        return tr("Classic");
    case Entry::Portals:
        return tr("Portals");
    case Entry::HotPotato:
        break;
    }
    return tr("Hot potato");
}

bool ModeListModel::locked(Entry entry) const
{
    const auto unlockable = unlockable_of(entry);
    return unlockable && !m_Unlocks.has(*unlockable);
}

bool ModeListModel::selectable(Entry entry)
{
    return game_mode_of(entry) >= 0;
}

} // namespace snake::ui
