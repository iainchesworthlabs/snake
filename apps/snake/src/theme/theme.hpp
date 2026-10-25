// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtQml/qqmlregistration.h>

#include <QColor>
#include <QFont>
#include <QLocale>
#include <QObject>
#include <QString>

namespace snake::ui
{

/// Loads the bundled fonts into the application. Returns false when any of them could not be loaded. Call it once,
/// after the GUI application exists and before the first window.
bool load_fonts();

/// The font the application uses for all text: the bundled family, then the bundled fonts for the scripts it lacks,
/// then the fallbacks of the design tokens. The Han characters of Japanese and Chinese are drawn differently, so the
/// font of the language comes first.
QFont application_font(const QLocale& locale = QLocale());

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
    Q_PROPERTY(QColor medalGold READ medalGold CONSTANT FINAL)
    Q_PROPERTY(QColor medalSilver READ medalSilver CONSTANT FINAL)
    Q_PROPERTY(QColor medalBronze READ medalBronze CONSTANT FINAL)
    Q_PROPERTY(QString fontFamily READ fontFamily CONSTANT FINAL)
    /// The language reads from right to left: the screens are mirrored.
    Q_PROPERTY(bool rightToLeft READ rightToLeft NOTIFY rightToLeftChanged FINAL)

public:
    explicit Theme(QObject* parent = nullptr);

    [[nodiscard]] QColor background() const;
    [[nodiscard]] QColor ink() const;
    [[nodiscard]] QColor shadow() const;
    [[nodiscard]] QColor surface() const;
    [[nodiscard]] QColor accent() const;
    /// The dimming drawn over the game while it is paused.
    [[nodiscard]] QColor overlay() const;
    /// The tiles of the first three places on the leaderboard.
    [[nodiscard]] QColor medalGold() const;
    [[nodiscard]] QColor medalSilver() const;
    [[nodiscard]] QColor medalBronze() const;
    [[nodiscard]] QString fontFamily() const;

    [[nodiscard]] static bool rightToLeft();

    /// The left edge of something `width` wide that is `x` from the left of a space `container` wide, or, when the
    /// language reads right to left, the same distance from the right. QML places most things with anchors, which
    /// mirror by themselves; this is for what has a plain x.
    Q_INVOKABLE [[nodiscard]] static double mirrored(double x, double width, double container);

signals:
    void rightToLeftChanged();
};

} // namespace snake::ui
