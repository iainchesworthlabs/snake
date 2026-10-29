// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/gamepad.hpp"
#include "snake/input/direction_selector.hpp"

#include <QtQml/qqmlregistration.h>

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <memory>

namespace snake::ui
{

/// Plays the game from a gamepad. It reads the pads of the computer (device::gamepad_source()) and turns each button
/// into the key that the keyboard has for it, sent to the window: the d-pad and the left stick are the arrow keys,
/// Start is Space (pause and resume), Select and the east button are Escape (the menu), and the south button is Enter
/// (confirm). So the game and every menu answer a pad as they answer the keyboard, and there is one set of rules for
/// both. It reads the pads on a timer, and sends nothing while its window is not the active one.
class GamepadController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    /// Where the keys go: the game's window.
    Q_PROPERTY(QObject* target READ target WRITE setTarget NOTIFY targetChanged FINAL)
    /// At least one pad is plugged in.
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged FINAL)

public:
    /// How often the pads are read, in milliseconds.
    static constexpr int POLL_MS = 8;

    /// Reads the gamepads of this device (device::gamepad_source()).
    explicit GamepadController(QObject* parent = nullptr);
    /// Reads `source`, or nothing when it is null. Tests give a recording one.
    explicit GamepadController(std::unique_ptr<device::GamepadSource> source, QObject* parent = nullptr);

    [[nodiscard]] QObject* target() const noexcept
    {
        return m_Target;
    }
    void setTarget(QObject* target);

    [[nodiscard]] bool connected() const noexcept
    {
        return m_Pads > 0;
    }

    /// Reads what the pads have done since the last time, and sends the keys. The timer calls it; tests call it.
    Q_INVOKABLE void poll();

signals:
    void targetChanged();
    void connectedChanged();

private:
    void on_event(const device::GamepadEvent& event);
    /// Sends a key press and release to the target, unless it is a window that is not the active one.
    void send_key(int key, bool pressed);
    [[nodiscard]] bool can_send() const;

    std::unique_ptr<device::GamepadSource> m_Source;
    QTimer m_Timer;
    QPointer<QObject> m_Target;
    input::DirectionSelector m_Stick;
    // How many pads are plugged in.
    int m_Pads = 0;
};

} // namespace snake::ui
