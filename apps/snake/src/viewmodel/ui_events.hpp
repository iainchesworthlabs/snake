// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtQml/qqmlregistration.h>

#include <QObject>

class QJSEngine;
class QQmlEngine;

namespace snake::ui
{

/// Things the screens tell the rest of the program that have no game or profile to carry them: a button was pressed.
/// There is one for the whole program, and every button of every screen reaches it as the QML singleton `UiEvents`.
class UiEvents : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    [[nodiscard]] static UiEvents& instance();
    /// The QML singleton is the one instance, not a copy for each engine.
    static UiEvents* create(QQmlEngine* engine, QJSEngine* script_engine);

signals:
    /// A button, switch or choice was pressed.
    void buttonPressed();

private:
    UiEvents() = default;
};

} // namespace snake::ui
