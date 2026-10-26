// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/input/direction_selector.hpp"
#include "snake/input/gamepad_map.hpp"
#include "snake/input/sample_source.hpp"
#include "snake/input/swipe_recogniser.hpp"
#include "snake/input/tilt_processor.hpp"
#include "snake/sim/direction.hpp"
#include "viewmodel/game_view_model.hpp"
#include "viewmodel/profile_view_model.hpp"

#include <QtQml/qqmlregistration.h>

#include <QObject>
#include <cstdint>
#include <memory>

namespace snake::ui
{

/// How a touch device is steered: by tilting it, by swiping the screen, or by an on-screen d-pad. It reads the choice
/// of the profile, the accelerometer of the device and the touches of the screen, and turns the snake of a
/// GameViewModel with libs/input's tilt processor and swipe recogniser. The keyboard needs none of it: it goes straight
/// to the game.
///
/// Tilt is calibrated at the start of every game, so the player holds the device however is comfortable, and again when
/// the pause dialog asks. A device with no accelerometer cannot be steered by tilt, so a choice of tilt is answered
/// with swiping there; a computer that has no touch screen has the keyboard.
class InputController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(snake::ui::GameViewModel* game READ game WRITE setGame NOTIFY gameChanged FINAL)
    Q_PROPERTY(snake::ui::ProfileViewModel* profile READ profile WRITE setProfile NOTIFY profileChanged FINAL)
    /// The device has a touch screen: only then is there anything but the keyboard.
    Q_PROPERTY(bool touchDevice READ touchDevice WRITE setTouchDevice NOTIFY controlsChanged FINAL)
    /// The device has an accelerometer, so tilt can be chosen.
    Q_PROPERTY(bool tiltAvailable READ tiltAvailable CONSTANT FINAL)
    /// How the snake is steered now, which is the profile's choice unless the device cannot do it.
    Q_PROPERTY(Scheme controls READ controls NOTIFY controlsChanged FINAL)
    /// How far the device is tipped from where it was calibrated, in degrees: positive right when the right edge is
    /// down, positive up when the top edge is down. The tilt indicator draws them.
    Q_PROPERTY(double tiltRight READ tiltRight NOTIFY tiltChanged FINAL)
    Q_PROPERTY(double tiltUp READ tiltUp NOTIFY tiltChanged FINAL)
    /// The way the tilt steers now, or Heading::None.
    Q_PROPERTY(Heading tiltHeading READ tiltHeading NOTIFY tiltChanged FINAL)
    /// The angles of the dead zone and of the turn, for the indicator.
    Q_PROPERTY(double deadZone READ deadZone CONSTANT FINAL)
    Q_PROPERTY(double threshold READ threshold CONSTANT FINAL)

public:
    enum class Scheme : std::uint8_t
    {
        Keys, // no touch screen: the keyboard
        Tilt,
        Swipe,
        Dpad,
        Joystick,
    };
    Q_ENUM(Scheme)

    enum class Heading : std::uint8_t
    {
        None,
        Up,
        Down,
        Left,
        Right,
    };
    Q_ENUM(Heading)

    /// Reads the accelerometer of this device (device::make_accelerometer()).
    explicit InputController(QObject* parent = nullptr);
    /// Reads `source`. Tests give a recording.
    explicit InputController(std::unique_ptr<input::SampleSource> source, QObject* parent = nullptr);

    [[nodiscard]] GameViewModel* game() const noexcept
    {
        return m_Game;
    }
    void setGame(GameViewModel* game);

    [[nodiscard]] ProfileViewModel* profile() const noexcept
    {
        return m_Profile;
    }
    void setProfile(ProfileViewModel* profile);

    [[nodiscard]] bool touchDevice() const noexcept
    {
        return m_TouchDevice;
    }
    void setTouchDevice(bool touch);

    [[nodiscard]] bool tiltAvailable() const noexcept
    {
        return m_Source != nullptr;
    }
    [[nodiscard]] Scheme controls() const;

    [[nodiscard]] double tiltRight() const noexcept
    {
        return static_cast<double>(m_Reading.right_degrees);
    }
    [[nodiscard]] double tiltUp() const noexcept
    {
        return static_cast<double>(m_Reading.up_degrees);
    }
    [[nodiscard]] Heading tiltHeading() const noexcept;
    [[nodiscard]] static double deadZone() noexcept
    {
        return static_cast<double>(input::TILT_SELECTOR.release);
    }
    [[nodiscard]] static double threshold() noexcept
    {
        return static_cast<double>(input::TILT_SELECTOR.engage);
    }

    /// Once a frame: takes in the accelerometer's readings, and while a game runs with tilt chosen steers by them. Call
    /// it before the game advances, so the turn is in place for the tick.
    Q_INVOKABLE void update();

    /// A d-pad button was pressed.
    Q_INVOKABLE void steer(Heading heading);

    /// Touches of the screen, in device-independent pixels, for the swipe. `pointer` tells the fingers apart.
    Q_INVOKABLE void touchPressed(int pointer, double x, double y);
    Q_INVOKABLE void touchMoved(int pointer, double x, double y);
    Q_INVOKABLE void touchReleased(int pointer);
    /// The system took the touches away.
    Q_INVOKABLE void touchCancelled();

    /// The knob of the on-screen joystick, as a fraction of the way it can travel: -1 to 1, right and up positive. A
    /// direction is set once the knob is far enough out (TOUCH_STICK_SELECTOR) and stays while it is not near the
    /// middle.
    Q_INVOKABLE void joystickMoved(double right, double up);
    /// The thumb came off the joystick.
    Q_INVOKABLE void joystickReleased();

signals:
    void gameChanged();
    void profileChanged();
    void controlsChanged();
    void tiltChanged();

private:
    void on_game_started();
    void on_settings_changed();

    std::unique_ptr<input::SampleSource> m_Source;
    input::TiltProcessor m_Tilt;
    input::TiltReading m_Reading;
    input::SwipeRecogniser m_Swipe;
    input::DirectionSelector m_Stick{input::TOUCH_STICK_SELECTOR};
    GameViewModel* m_Game = nullptr;
    ProfileViewModel* m_Profile = nullptr;
    bool m_TouchDevice = false;
};

} // namespace snake::ui
