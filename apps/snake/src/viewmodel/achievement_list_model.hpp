// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/profile/achievements.hpp"

#include <QtQml/qqmlregistration.h>

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QModelIndex>
#include <QString>
#include <QVariant>

namespace snake::ui
{

/// The twelve achievements, one row each, in the order of profile::Achievement: its name and what it takes, whether it
/// is earned, and for the two that count blueberries how far along the count is. What is earned comes from the
/// Achievements the model is given.
class AchievementListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The profile owns the achievement list.")

public:
    enum Role
    {
        /// The number of the achievement, 0 to 11, as profile::Achievement numbers it.
        NumberRole = Qt::UserRole + 1,
        NameRole,
        /// What it takes to earn it, in a sentence.
        DescriptionRole,
        /// True once the achievement is earned.
        EarnedRole,
        /// How far along a counted achievement is, such as "37 / 100". Empty for the others.
        ProgressRole,
    };
    Q_ENUM(Role)

    /// `achievements` must outlive the model. Call refresh() when it changes.
    explicit AchievementListModel(const profile::Achievements& achievements, QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    /// The achievements changed: every row is read again.
    void refresh();

    /// The name of an achievement, as the player sees it.
    [[nodiscard]] static QString name_of(profile::Achievement achievement);
    /// What it takes to earn an achievement, as the player reads it.
    [[nodiscard]] static QString description_of(profile::Achievement achievement);

private:
    const profile::Achievements& m_Achievements;
};

} // namespace snake::ui
