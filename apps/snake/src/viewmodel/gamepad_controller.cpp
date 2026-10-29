// SPDX-License-Identifier: GPL-3.0-or-later
#include "viewmodel/gamepad_controller.hpp"

#include "device/gamepad.hpp"
#include "snake/input/gamepad_map.hpp"
#include "snake/sim/direction.hpp"

#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QWindow>
#include <memory>
#include <optional>
#include <utility>

namespace snake::ui
{

namespace
{

/// The arrow key for a direction.
int key_of(sim::Direction direction) noexcept
{
    switch (direction)
    {
    case sim::Direction::UP:
        return Qt::Key_Up;
    case sim::Direction::DOWN:
        return Qt::Key_Down;
    case sim::Direction::LEFT:
        return Qt::Key_Left;
    case sim::Direction::RIGHT:
        return Qt::Key_Right;
    }
    return Qt::Key_unknown;
}

/// The key for a button that is not a d-pad button, or none for a button that is: the keyboard's pause, menu and
/// confirm.
std::optional<int> key_of(input::GamepadButton button) noexcept
{
    if (const auto steer = input::steer_for(button))
    {
        return key_of(*steer);
    }
    if (const auto action = input::action_for(button))
    {
        switch (*action)
        {
        case input::Action::PAUSE_RESUME:
            return Qt::Key_Space;
        case input::Action::MENU:
            return Qt::Key_Escape;
        case input::Action::CONFIRM:
            return Qt::Key_Return;
        }
    }
    return std::nullopt;
}

} // namespace

GamepadController::GamepadController(QObject* parent) : GamepadController(device::gamepad_source(), parent) {}

GamepadController::GamepadController(std::unique_ptr<device::GamepadSource> source, QObject* parent)
    : QObject(parent), m_Source(std::move(source)), m_Stick(input::STICK_SELECTOR)
{
    m_Timer.setInterval(POLL_MS);
    m_Timer.setTimerType(Qt::PreciseTimer);
    connect(&m_Timer, &QTimer::timeout, this, &GamepadController::poll);
    if (m_Source != nullptr)
    {
        m_Timer.start();
    }
}

void GamepadController::setTarget(QObject* target)
{
    if (target == m_Target)
    {
        return;
    }
    m_Target = target;
    emit targetChanged();
}

void GamepadController::poll()
{
    if (m_Source == nullptr)
    {
        return;
    }
    m_Source->poll([this](const device::GamepadEvent& event) { on_event(event); });
}

void GamepadController::on_event(const device::GamepadEvent& event)
{
    using Kind = device::GamepadEvent::Kind;
    switch (event.kind)
    {
    case Kind::CONNECTED:
        ++m_Pads;
        if (m_Pads == 1)
        {
            emit connectedChanged();
        }
        break;
    case Kind::DISCONNECTED:
        if (m_Pads > 0 && --m_Pads == 0)
        {
            emit connectedChanged();
        }
        break;
    case Kind::BUTTON_DOWN:
    case Kind::BUTTON_UP:
        if (const auto key = key_of(event.button))
        {
            send_key(*key, event.kind == Kind::BUTTON_DOWN);
        }
        break;
    case Kind::STICK:
    {
        // The stick turns the way it is pushed, once for each push: a direction that is held does not repeat.
        const auto before = m_Stick.current();
        const auto now = m_Stick.update(event.right, event.up);
        if (now && now != before)
        {
            send_key(key_of(*now), true);
            send_key(key_of(*now), false);
        }
        break;
    }
    }
}

bool GamepadController::can_send() const
{
    if (m_Target == nullptr)
    {
        return false;
    }
    // A window that is not the one in use hears nothing: the pad may be in another program's hands.
    const auto* window = qobject_cast<const QWindow*>(m_Target.data());
    return window == nullptr || window->isActive();
}

void GamepadController::send_key(int key, bool pressed)
{
    if (!can_send())
    {
        return;
    }
    QKeyEvent event(pressed ? QEvent::KeyPress : QEvent::KeyRelease, key, Qt::NoModifier);
    QCoreApplication::sendEvent(m_Target, &event);
}

} // namespace snake::ui
