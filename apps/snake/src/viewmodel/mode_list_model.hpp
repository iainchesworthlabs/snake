// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/profile/unlocks.hpp"

#include <QtQml/qqmlregistration.h>

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QModelIndex>
#include <QVariant>
#include <cstdint>

namespace snake::ui
{

/// The modes of the title screen, in the order they are listed: the two that are always open and the two that are shown
/// with what it takes to open them until they are. Whether a mode is open comes from the unlocks the model is given.
class ModeListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The profile owns the mode list.")

public:
    enum class Entry : std::uint8_t
    {
        Easy,
        Classic,
        Portals,
        HotPotato,
    };
    Q_ENUM(Entry)

    enum Role
    {
        EntryRole = Qt::UserRole + 1,
        NameRole,
        DescriptionRole,
        /// True for a mode that is not open yet. Its unlock hint says what it takes.
        LockedRole,
        /// True for a mode the player can choose: one that is open.
        SelectableRole,
        /// The text under the name: what the mode is, or what it takes to open it.
        DetailRole,
        /// The GameViewModel::Mode of a selectable entry, and -1 for any other.
        GameModeRole,
    };
    Q_ENUM(Role)

    /// `unlocks` must outlive the model. Call refresh() when it changes.
    explicit ModeListModel(const profile::Unlocks& unlocks, QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    /// The unlocks changed: every row is read again.
    void refresh();

    /// The name of an entry, as the player sees it.
    [[nodiscard]] static QString name_of(Entry entry);

private:
    [[nodiscard]] bool locked(Entry entry) const;
    [[nodiscard]] bool selectable(Entry entry) const;

    const profile::Unlocks& m_Unlocks;
};

} // namespace snake::ui
