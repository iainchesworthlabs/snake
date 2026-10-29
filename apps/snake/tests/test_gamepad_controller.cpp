// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/gamepad.hpp"
#include "scripted_gamepad.hpp"
#include "snake/input/gamepad_map.hpp"
#include "viewmodel/gamepad_controller.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QEvent>
#include <QKeyEvent>
#include <QObject>
#include <QSignalSpy>
#include <QTest>
#include <QVector>
#include <memory>
#include <utility>
#include <vector>

using snake::input::GamepadButton;
using snake::ui::GamepadController;
using snake::ui::device::GamepadEvent;
using snake::ui::device::GamepadSource;
using Kind = GamepadEvent::Kind;

namespace
{

/// Writes down the key events it is sent: "+" for a press and "-" for a release, then the key.
class KeyCatcher final : public QObject
{
public:
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
        {
            const auto* key = static_cast<QKeyEvent*>(event);
            keys.push_back(QString(event->type() == QEvent::KeyPress ? "+" : "-") + QString::number(key->key()));
            return true;
        }
        return QObject::event(event);
    }

    QStringList keys;
};

GamepadEvent down(GamepadButton button)
{
    return {.kind = Kind::BUTTON_DOWN, .button = button};
}

GamepadEvent up(GamepadButton button)
{
    return {.kind = Kind::BUTTON_UP, .button = button};
}

GamepadEvent stick(float right, float up_)
{
    return {.kind = Kind::STICK, .right = right, .up = up_};
}

QString pressed(int key)
{
    return "+" + QString::number(key);
}

QString released(int key)
{
    return "-" + QString::number(key);
}

/// A controller reading `script`, sending keys to `catcher`.
struct Rig
{
    std::vector<GamepadEvent> script;
    KeyCatcher catcher;
    GamepadController controller{std::make_unique<snake::test::ScriptedGamepad>(&script)};

    Rig()
    {
        controller.setTarget(&catcher);
    }

    /// Plays `events` through the controller.
    void play(std::vector<GamepadEvent> events)
    {
        script = std::move(events);
        controller.poll();
    }
};

} // namespace

TEST_CASE("The d-pad is the arrow keys, pressed and released with the button", "[gamepad]")
{
    Rig rig;
    rig.play({down(GamepadButton::DPAD_UP), up(GamepadButton::DPAD_UP), down(GamepadButton::DPAD_DOWN),
              down(GamepadButton::DPAD_LEFT), down(GamepadButton::DPAD_RIGHT), up(GamepadButton::DPAD_RIGHT)});
    CHECK(rig.catcher.keys == QStringList{pressed(Qt::Key_Up), released(Qt::Key_Up), pressed(Qt::Key_Down),
                                          pressed(Qt::Key_Left), pressed(Qt::Key_Right), released(Qt::Key_Right)});
}

TEST_CASE("Start is Space, Select and the east button are Escape, and the south button is Enter", "[gamepad]")
{
    Rig rig;
    rig.play({down(GamepadButton::START), up(GamepadButton::START), down(GamepadButton::SELECT),
              down(GamepadButton::EAST), down(GamepadButton::SOUTH), up(GamepadButton::SOUTH)});
    CHECK(rig.catcher.keys == QStringList{pressed(Qt::Key_Space), released(Qt::Key_Space), pressed(Qt::Key_Escape),
                                          pressed(Qt::Key_Escape), pressed(Qt::Key_Return), released(Qt::Key_Return)});
}

TEST_CASE("The left stick turns once for each push, and not again while it is held", "[gamepad][stick]")
{
    Rig rig;
    rig.play({stick(0.3F, 0.0F)}); // not far enough
    CHECK(rig.catcher.keys.isEmpty());

    rig.play({stick(0.8F, 0.0F)});
    CHECK(rig.catcher.keys == QStringList{pressed(Qt::Key_Right), released(Qt::Key_Right)});
    rig.catcher.keys.clear();

    rig.play({stick(0.9F, 0.1F), stick(1.0F, 0.0F)}); // still right
    CHECK(rig.catcher.keys.isEmpty());

    rig.play({stick(0.0F, 0.0F), stick(0.0F, 0.9F)}); // let go, then up
    CHECK(rig.catcher.keys == QStringList{pressed(Qt::Key_Up), released(Qt::Key_Up)});
    rig.catcher.keys.clear();

    // From up straight to left, without passing the middle: left is a new direction.
    rig.play({stick(-0.9F, 0.0F)});
    CHECK(rig.catcher.keys == QStringList{pressed(Qt::Key_Left), released(Qt::Key_Left)});
}

TEST_CASE("A stick that is pushed down turns the snake down", "[gamepad][stick]")
{
    Rig rig;
    rig.play({stick(0.0F, -0.9F)});
    CHECK(rig.catcher.keys == QStringList{pressed(Qt::Key_Down), released(Qt::Key_Down)});
}

TEST_CASE("The controller says when a pad is there, and when the last one is gone", "[gamepad]")
{
    Rig rig;
    QSignalSpy changed(&rig.controller, &GamepadController::connectedChanged);
    CHECK_FALSE(rig.controller.connected());

    rig.play({{.kind = Kind::CONNECTED}});
    CHECK(rig.controller.connected());
    CHECK(changed.count() == 1);

    rig.play({{.kind = Kind::CONNECTED}}); // a second pad changes nothing to say
    CHECK(changed.count() == 1);

    rig.play({{.kind = Kind::DISCONNECTED}});
    CHECK(rig.controller.connected());
    rig.play({{.kind = Kind::DISCONNECTED}});
    CHECK_FALSE(rig.controller.connected());
    CHECK(changed.count() == 2);

    rig.play({{.kind = Kind::DISCONNECTED}}); // more goodbyes than hellos do no harm
    CHECK_FALSE(rig.controller.connected());
    CHECK(changed.count() == 2);
}

TEST_CASE("Nothing is sent without a target, and a target that is deleted is forgotten", "[gamepad]")
{
    Rig rig;
    rig.controller.setTarget(nullptr);
    rig.play({down(GamepadButton::START)});
    CHECK(rig.catcher.keys.isEmpty());

    auto* gone = new KeyCatcher;
    rig.controller.setTarget(gone);
    delete gone;
    CHECK(rig.controller.target() == nullptr);
    rig.play({down(GamepadButton::START)});
}

TEST_CASE("A controller with no gamepads reads nothing and says nothing", "[gamepad]")
{
    GamepadController controller{std::unique_ptr<GamepadSource>{}};
    KeyCatcher catcher;
    controller.setTarget(&catcher);
    controller.poll();
    CHECK_FALSE(controller.connected());
    CHECK(catcher.keys.isEmpty());
}

TEST_CASE("The controller reads its pads by itself, on a timer", "[gamepad]")
{
    Rig rig;
    rig.script = {down(GamepadButton::SOUTH)};
    CHECK(QTest::qWaitFor([&] { return !rig.catcher.keys.isEmpty(); }, 2000));
    CHECK(rig.catcher.keys == QStringList{pressed(Qt::Key_Return)});
}

TEST_CASE("The gamepad source can be replaced, and put back", "[gamepad]")
{
    using snake::ui::device::set_gamepad_factory;
    std::vector<GamepadEvent> script;
    set_gamepad_factory([&script] { return std::make_unique<snake::test::ScriptedGamepad>(&script); });
    CHECK(snake::ui::device::gamepad_source() != nullptr);
    set_gamepad_factory({});
}
