// SPDX-License-Identifier: GPL-3.0-or-later
#include "viewmodel/input_controller.hpp"

#include "device/device.hpp"
#include "device/haptics.hpp"
#include "snake/input/joystick_steering.hpp"
#include "snake/input/sample_source.hpp"
#include "snake/input/swipe_recogniser.hpp"
#include "snake/input/tilt_processor.hpp"
#include "snake/sim/direction.hpp"
#include "snake/sim/simulation.hpp"
#include "viewmodel/game_view_model.hpp"
#include "viewmodel/profile_view_model.hpp"

#include <QObject>
#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

namespace snake::ui
{

namespace
{

sim::Direction to_direction(InputController::Heading heading)
{
    switch (heading)
    {
    case InputController::Heading::Down:
        return sim::Direction::DOWN;
    case InputController::Heading::Left:
        return sim::Direction::LEFT;
    case InputController::Heading::Right:
        return sim::Direction::RIGHT;
    case InputController::Heading::Up:
    case InputController::Heading::None:
        break;
    }
    return sim::Direction::UP;
}

InputController::Heading to_heading(sim::Direction direction)
{
    switch (direction)
    {
    case sim::Direction::DOWN:
        return InputController::Heading::Down;
    case sim::Direction::LEFT:
        return InputController::Heading::Left;
    case sim::Direction::RIGHT:
        return InputController::Heading::Right;
    case sim::Direction::UP:
        break;
    }
    return InputController::Heading::Up;
}

} // namespace

/// Passes the readings on and remembers the latest, for the readout.
class InputController::RecordingSource final : public input::SampleSource
{
public:
    explicit RecordingSource(std::unique_ptr<input::SampleSource> inner) : m_Inner(std::move(inner)) {}

    std::optional<input::AccelSample> next() noexcept override
    {
        auto sample = m_Inner->next();
        if (sample)
        {
            m_Last = *sample;
            ++m_Count;
        }
        return sample;
    }

    [[nodiscard]] input::AccelSample last() const noexcept
    {
        return m_Last;
    }
    [[nodiscard]] int count() const noexcept
    {
        return m_Count;
    }

private:
    std::unique_ptr<input::SampleSource> m_Inner;
    input::AccelSample m_Last{};
    int m_Count = 0;
};

InputController::InputController(QObject* parent) : InputController(device::accelerometer(), parent) {}

InputController::InputController(std::unique_ptr<input::SampleSource> source, QObject* parent) : QObject(parent)
{
    if (source != nullptr)
    {
        auto recording = std::make_unique<RecordingSource>(std::move(source));
        m_Recording = recording.get();
        m_Source = std::move(recording);
    }
}

void InputController::setGame(GameViewModel* game)
{
    if (game == m_Game)
    {
        return;
    }
    if (m_Game != nullptr)
    {
        disconnect(m_Game, nullptr, this, nullptr);
    }
    m_Game = game;
    if (m_Game != nullptr)
    {
        connect(m_Game, &GameViewModel::gameStarted, this, &InputController::on_game_started);
        connect(m_Game, &GameViewModel::stateChanged, this,
                [this]
                {
                    m_Swipe.cancel();
                    // A game that is no longer held (it was left, restarted or ended) takes the calibration with it.
                    if (m_Calibrating && m_Game->state() != GameViewModel::State::Paused)
                    {
                        cancel_calibration();
                    }
                });
    }
    emit gameChanged();
}

void InputController::setProfile(ProfileViewModel* profile)
{
    if (profile == m_Profile)
    {
        return;
    }
    if (m_Profile != nullptr)
    {
        disconnect(m_Profile, nullptr, this, nullptr);
    }
    m_Profile = profile;
    if (m_Profile != nullptr)
    {
        connect(m_Profile, &ProfileViewModel::settingsChanged, this, &InputController::on_settings_changed);
        connect(m_Profile, &ProfileViewModel::calibrationRequested, this, [this] { startCalibration(); });
    }
    on_settings_changed();
    emit profileChanged();
}

void InputController::setTouchDevice(bool touch)
{
    if (touch == m_TouchDevice)
    {
        return;
    }
    m_TouchDevice = touch;
    emit controlsChanged();
}

InputController::Scheme InputController::controls() const
{
    if (!m_TouchDevice)
    {
        return Scheme::Keys;
    }
    const ProfileViewModel::Controls chosen =
        m_Profile != nullptr ? m_Profile->controls() : ProfileViewModel::Controls::Tilt;
    switch (chosen)
    {
    case ProfileViewModel::Controls::Dpad:
        return Scheme::Dpad;
    case ProfileViewModel::Controls::Swipe:
        return Scheme::Swipe;
    case ProfileViewModel::Controls::Joystick:
        return Scheme::Joystick;
    case ProfileViewModel::Controls::Tilt:
        break;
    }
    // A device that cannot feel its tilt is steered by swiping.
    return tiltAvailable() ? Scheme::Tilt : Scheme::Swipe;
}

InputController::Heading InputController::tiltHeading() const noexcept
{
    return m_Reading.direction ? to_heading(*m_Reading.direction) : Heading::None;
}

void InputController::on_settings_changed()
{
    m_Tilt.set_invert_axes(m_Profile != nullptr && m_Profile->invertAxes());
    input::SelectorConfig selector = input::TILT_SELECTOR;
    if (m_Profile != nullptr)
    {
        switch (m_Profile->tiltSensitivity())
        {
        case ProfileViewModel::TiltSensitivity::Quick:
            selector = input::TILT_SELECTOR_QUICK;
            break;
        case ProfileViewModel::TiltSensitivity::Gentle:
            selector = input::TILT_SELECTOR_GENTLE;
            break;
        case ProfileViewModel::TiltSensitivity::Normal:
            break;
        }
    }
    m_Tilt.set_selector(selector);
    emit tiltSettingsChanged();
    // The choice of the profile may have changed what steers.
    m_Swipe.cancel();
    m_Stick.release();
    emit controlsChanged();
}

void InputController::on_game_started()
{
    m_Stick.release();
    // A game begins with the phone held as the player likes: that is neutral, taken after a short wait that the screen
    // shows.
    startCalibration();
}

void InputController::startCalibration()
{
    if (m_Game == nullptr || controls() != Scheme::Tilt)
    {
        return;
    }
    m_CalibrationElapsed = 0.0;
    m_CalibrationProgress = 0.0;
    if (!m_Calibrating)
    {
        m_Calibrating = true;
        emit calibratingChanged();
    }
    emit calibrationProgressChanged();
    // The game is held while the player gets the phone how they like it.
    if (m_Game->state() == GameViewModel::State::Running)
    {
        m_Game->pause();
    }
}

void InputController::resume()
{
    if (m_Game == nullptr)
    {
        return;
    }
    if (controls() == Scheme::Tilt)
    {
        startCalibration();
        return;
    }
    m_Game->resume();
}

void InputController::finish_calibration()
{
    m_Tilt.calibrate();
    m_Calibrating = false;
    m_CalibrationProgress = 1.0;
    emit calibrationProgressChanged();
    emit calibratingChanged();
    // A tap, so that the player feels it happen as well as sees it.
    if (device::Haptics* haptics = device::default_haptics(); haptics != nullptr)
    {
        haptics->tap();
    }
    if (m_Game != nullptr && m_Game->state() == GameViewModel::State::Paused)
    {
        m_Game->resume();
    }
}

void InputController::cancel_calibration()
{
    if (!m_Calibrating)
    {
        return;
    }
    m_Calibrating = false;
    m_CalibrationProgress = 0.0;
    emit calibrationProgressChanged();
    emit calibratingChanged();
}

void InputController::update(double seconds)
{
    if (controls() == Scheme::Joystick)
    {
        // A thumb that is held still still steers: the heading it is read against changes with each turn.
        steer_by_joystick();
    }
    if (m_Source == nullptr)
    {
        return;
    }
    // The readings are taken in whatever the game is doing, so the filter is up to date when a game begins.
    const input::TiltReading reading = m_Tilt.drain(*m_Source);
    if (m_Recording != nullptr && m_Recording->count() != m_RawCount)
    {
        m_RawCount = m_Recording->count();
        m_Raw = m_Recording->last();
        emit rawChanged();
    }
    const bool changed = reading.right_degrees != m_Reading.right_degrees ||
                         reading.up_degrees != m_Reading.up_degrees || reading.direction != m_Reading.direction;
    m_Reading = reading;
    if (changed)
    {
        emit tiltChanged();
    }
    if (m_Calibrating)
    {
        // The wait runs on the time of the frames, and the filter above settles on the pose meanwhile.
        m_CalibrationElapsed += seconds;
        m_CalibrationProgress = std::min(1.0, m_CalibrationElapsed / CALIBRATION_SECONDS);
        emit calibrationProgressChanged();
        if (m_CalibrationElapsed >= CALIBRATION_SECONDS)
        {
            finish_calibration();
        }
        return;
    }
    if (m_Game != nullptr && m_Game->state() == GameViewModel::State::Running && controls() == Scheme::Tilt &&
        reading.direction)
    {
        // The direction in force, offered every frame: the input buffer drops the repeats.
        static_cast<void>(m_Game->turn(*reading.direction));
    }
}

void InputController::steer(Heading heading)
{
    if (m_Game != nullptr && heading != Heading::None && controls() == Scheme::Dpad)
    {
        static_cast<void>(m_Game->turn(to_direction(heading)));
    }
}

void InputController::touchPressed(int pointer, double x, double y)
{
    if (controls() == Scheme::Swipe)
    {
        m_Swipe.press(pointer, {static_cast<float>(x), static_cast<float>(y)});
    }
}

void InputController::touchMoved(int pointer, double x, double y)
{
    if (m_Game == nullptr || controls() != Scheme::Swipe)
    {
        return;
    }
    if (const auto direction = m_Swipe.move(pointer, {static_cast<float>(x), static_cast<float>(y)}))
    {
        static_cast<void>(m_Game->turn(*direction));
    }
}

void InputController::touchReleased(int pointer)
{
    m_Swipe.release(pointer);
}

void InputController::touchCancelled()
{
    m_Swipe.cancel();
}

void InputController::joystickMoved(double right, double up)
{
    if (m_Game == nullptr || controls() != Scheme::Joystick)
    {
        return;
    }
    m_Stick.hold(static_cast<float>(right), static_cast<float>(up));
    steer_by_joystick();
}

void InputController::joystickReleased()
{
    m_Stick.release();
}

void InputController::steer_by_joystick()
{
    if (m_Game == nullptr || m_Game->state() != GameViewModel::State::Running)
    {
        return;
    }
    // The input buffer drops a repeat of the direction already in force.
    if (const auto turn = m_Stick.again(m_Game->session().simulation()))
    {
        static_cast<void>(m_Game->turn(*turn));
    }
}

} // namespace snake::ui
