// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtQml/qqmlregistration.h>

#include <QColor>
#include <QFont>
#include <QObject>
#include <QString>

namespace snake::ui
{

/// Loads the bundled fonts into the application. Returns false when any of them could not be loaded. Call it once,
/// after the GUI application exists and before the first window.
bool load_fonts();

/// The font the application uses for all text: the bundled family, then the fallbacks of the design tokens.
QFont application_font();

/// The design tokens as QML sees them: `Theme.ink`, `Theme.fontFamily` and so on. The values are constants.
class Theme : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QColor background READ background CONSTANT FINAL)
    Q_PROPERTY(QColor ink READ ink CONSTANT FINAL)
    Q_PROPERTY(QColor shadow READ shadow CONSTANT FINAL)
    Q_PROPERTY(QColor surface READ surface CONSTANT FINAL)
    Q_PROPERTY(QColor accent READ accent CONSTANT FINAL)
    Q_PROPERTY(QColor overlay READ overlay CONSTANT FINAL)
    Q_PROPERTY(QString fontFamily READ fontFamily CONSTANT FINAL)

public:
    explicit Theme(QObject* parent = nullptr);

    [[nodiscard]] QColor background() const;
    [[nodiscard]] QColor ink() const;
    [[nodiscard]] QColor shadow() const;
    [[nodiscard]] QColor surface() const;
    [[nodiscard]] QColor accent() const;
    /// The dimming drawn over the game while it is paused.
    [[nodiscard]] QColor overlay() const;
    [[nodiscard]] QString fontFamily() const;
};

} // namespace snake::ui
