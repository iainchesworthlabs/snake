// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/input/eight_way_selector.hpp"
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
    Q_PROPERTY(double deadZone READ deadZone NOTIFY tiltSettingsChanged FINAL)
    Q_PROPERTY(double threshold READ threshold NOTIFY tiltSettingsChanged FINAL)
    /// The latest reading of the accelerometer as it came, in g (flat and face up is z of 1), and how many have come.
    /// For the readout that tapping the tilt indicator shows, which tells a bug report what the sensor says.
    Q_PROPERTY(double rawX READ rawX NOTIFY rawChanged FINAL)
    Q_PROPERTY(double rawY READ rawY NOTIFY rawChanged FINAL)
    Q_PROPERTY(double rawZ READ rawZ NOTIFY rawChanged FINAL)
    Q_PROPERTY(int rawCount READ rawCount NOTIFY rawChanged FINAL)
    /// True while the phone is being calibrated: a short wait, with the game held, in which the player holds the phone
    /// the way they like to play and the pose is taken as neutral. It follows the start of a game and a resume from the
    /// pause dialog when tilt steers, and the dialog's Recalibrate tilt.
    Q_PROPERTY(bool calibrating READ calibrating NOTIFY calibratingChanged FINAL)
    /// How far through it is, 0 to 1.
    Q_PROPERTY(double calibrationProgress READ calibrationProgress NOTIFY calibrationProgressChanged FINAL)

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
    /// The angles, which follow the tilt sensitivity the profile has chosen.
    [[nodiscard]] double deadZone() const noexcept
    {
        return static_cast<double>(m_Tilt.config().selector.release);
    }
    [[nodiscard]] double threshold() const noexcept
    {
        return static_cast<double>(m_Tilt.config().selector.engage);
    }

    /// Once a frame: takes in the accelerometer's readings, and while a game runs with tilt chosen steers by them. Call
    /// it before the game advances, so the turn is in place for the tick. `seconds` is how long the frame took: it
    /// moves a calibration on.
    Q_INVOKABLE void update(double seconds = 0.0);

    /// How long a calibration takes: long enough for the filter to settle on the pose and for the player to see it
    /// happen.
    static constexpr double CALIBRATION_SECONDS = 1.5;

    [[nodiscard]] double rawX() const noexcept
    {
        return static_cast<double>(m_Raw.x);
    }
    [[nodiscard]] double rawY() const noexcept
    {
        return static_cast<double>(m_Raw.y);
    }
    [[nodiscard]] double rawZ() const noexcept
    {
        return static_cast<double>(m_Raw.z);
    }
    [[nodiscard]] int rawCount() const noexcept
    {
        return m_RawCount;
    }

    [[nodiscard]] bool calibrating() const noexcept
    {
        return m_Calibrating;
    }
    [[nodiscard]] double calibrationProgress() const noexcept
    {
        return m_CalibrationProgress;
    }

    /// Resumes a paused game. With tilt chosen the game stays held while the phone is calibrated, and then carries on;
    /// otherwise it resumes at once.
    Q_INVOKABLE void resume();

    /// Calibrates tilt after the wait, then resumes the game if it was held for it. Does nothing when tilt does not
    /// steer.
    Q_INVOKABLE void startCalibration();

    /// A d-pad button was pressed.
    Q_INVOKABLE void steer(Heading heading);

    /// Touches of the screen, in device-independent pixels, for the swipe. `pointer` tells the fingers apart.
    Q_INVOKABLE void touchPressed(int pointer, double x, double y);
    Q_INVOKABLE void touchMoved(int pointer, double x, double y);
    Q_INVOKABLE void touchReleased(int pointer);
    /// The system took the touches away.
    Q_INVOKABLE void touchCancelled();

    /// The knob of the on-screen joystick, as a fraction of the way it can travel: -1 to 1, right and up positive. It
    /// reads in eight ways (input::EightWaySelector): straight is that way, and a diagonal is a turn across the snake's
    /// line. The knob is read when it moves and again every frame (update()), so a thumb held on a diagonal steers a
    /// staircase.
    Q_INVOKABLE void joystickMoved(double right, double up);
    /// The thumb came off the joystick.
    Q_INVOKABLE void joystickReleased();

signals:
    void gameChanged();
    void profileChanged();
    void controlsChanged();
    void tiltChanged();
    void tiltSettingsChanged();
    void rawChanged();
    void calibratingChanged();
    void calibrationProgressChanged();

private:
    class RecordingSource;

    void on_game_started();
    void finish_calibration();
    void cancel_calibration();
    /// Reads the joystick's knob and offers the turn it asks for, if there is one.
    void steer_by_joystick();
    void on_settings_changed();

    std::unique_ptr<input::SampleSource> m_Source;
    RecordingSource* m_Recording = nullptr; ///< m_Source, when there is one.
    input::TiltProcessor m_Tilt;
    input::TiltReading m_Reading;
    input::SwipeRecogniser m_Swipe;
    input::EightWaySelector m_Stick{input::TOUCH_JOYSTICK};
    // Where the knob is, while a thumb is on it.
    float m_StickRight = 0.0F;
    float m_StickUp = 0.0F;
    bool m_StickHeld = false;
    input::AccelSample m_Raw{};
    int m_RawCount = 0;
    bool m_Calibrating = false;
    double m_CalibrationElapsed = 0.0;
    double m_CalibrationProgress = 0.0;
    GameViewModel* m_Game = nullptr;
    ProfileViewModel* m_Profile = nullptr;
    bool m_TouchDevice = false;
};

} // namespace snake::ui
