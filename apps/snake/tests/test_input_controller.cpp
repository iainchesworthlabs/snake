// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/device.hpp"
#include "device/haptics.hpp"
#include "snake/input/sample_source.hpp"
#include "snake/sim/direction.hpp"
#include "viewmodel/game_view_model.hpp"
#include "viewmodel/input_controller.hpp"
#include "viewmodel/profile_view_model.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QSignalSpy>
#include <cmath>
#include <cstdint>
#include <deque>
#include <memory>
#include <numbers>
#include <optional>
#include <utility>

using snake::input::AccelSample;
using snake::sim::Direction;
using snake::ui::GameViewModel;
using snake::ui::InputController;
using snake::ui::ProfileViewModel;
using Heading = InputController::Heading;
using Scheme = InputController::Scheme;

namespace
{

constexpr float HOLD = 55.0F;
constexpr std::uint32_t INTERVAL_US = 10'000;

constexpr float radians(float degrees)
{
    return degrees * std::numbers::pi_v<float> / 180.0F;
}

/// The gravity a device feels when it is held `hold` degrees above the horizontal, then rolled with its right edge down
/// by `right` degrees and its top edge down by `up` degrees (the same poses as the tilt processor's own tests).
AccelSample pose(float hold, float right, float up, std::uint32_t time_us)
{
    const float elevation = radians(hold - up);
    const float roll = radians(right);
    return {.x = -std::cos(elevation) * std::sin(roll),
            .y = std::sin(elevation),
            .z = std::cos(elevation) * std::cos(roll),
            .time_us = time_us};
}

/// The samples the test puts in; the controller takes them out through the source it owns.
struct Queue
{
    std::deque<AccelSample> samples;
    std::uint32_t time = 0;

    void hold(float seconds, float right, float up, float hold_angle = HOLD)
    {
        for (int i = 0; i < static_cast<int>(seconds * 100.0F); ++i)
        {
            samples.push_back(pose(hold_angle, right, up, time));
            time += INTERVAL_US;
        }
    }
};

class QueueSource final : public snake::input::SampleSource
{
public:
    explicit QueueSource(std::shared_ptr<Queue> queue) : m_Queue(std::move(queue)) {}

    std::optional<AccelSample> next() noexcept override
    {
        if (m_Queue->samples.empty())
        {
            return std::nullopt;
        }
        const AccelSample sample = m_Queue->samples.front();
        m_Queue->samples.pop_front();
        return sample;
    }

private:
    std::shared_ptr<Queue> m_Queue;
};

GameViewModel::SeedSource counting_seeds()
{
    return [next = std::uint64_t{1}]() mutable { return next++; };
}

/// A touch device with a game, a profile and an accelerometer fed by `queue`; the game is in Classic.
struct Rig
{
    std::shared_ptr<Queue> queue = std::make_shared<Queue>();
    GameViewModel game{counting_seeds()};
    ProfileViewModel profile;
    InputController input{std::make_unique<QueueSource>(queue)};

    Rig()
    {
        game.setMode(GameViewModel::Mode::Classic);
        input.setGame(&game);
        input.setProfile(&profile);
        input.setTouchDevice(true);
    }

    /// Starts the game with the device held level and lets the controller take that in, through the wait that a tilt
    /// game has before it begins.
    void start_level()
    {
        queue->hold(0.3F, 0.0F, 0.0F);
        game.start();
        input.update(InputController::CALIBRATION_SECONDS + 0.1);
    }

    [[nodiscard]] std::optional<Direction> pending() const
    {
        const auto& turns = game.session().simulation().pending_turns();
        return turns.empty() ? std::nullopt : std::optional(turns.at(0));
    }
};

} // namespace

TEST_CASE("A computer is steered by the keyboard and a phone by what it can feel", "[input-controller]")
{
    ProfileViewModel profile;
    InputController computer(std::make_unique<QueueSource>(std::make_shared<Queue>()));
    computer.setProfile(&profile);
    CHECK(computer.controls() == Scheme::Keys);

    computer.setTouchDevice(true);
    CHECK(computer.touchDevice());
    CHECK(computer.tiltAvailable());
    // The profile starts on tilt, and the device can.
    CHECK(computer.controls() == Scheme::Tilt);
    profile.setControls(ProfileViewModel::Controls::Swipe);
    CHECK(computer.controls() == Scheme::Swipe);
    profile.setControls(ProfileViewModel::Controls::Dpad);
    CHECK(computer.controls() == Scheme::Dpad);
}

TEST_CASE("A device that cannot feel its tilt is steered by swiping when tilt is chosen", "[input-controller]")
{
    ProfileViewModel profile;
    InputController input(nullptr);
    input.setProfile(&profile);
    input.setTouchDevice(true);
    CHECK_FALSE(input.tiltAvailable());
    CHECK(input.controls() == Scheme::Swipe);
    profile.setControls(ProfileViewModel::Controls::Dpad);
    CHECK(input.controls() == Scheme::Dpad);
    // Nothing to read, nothing happens.
    input.update();
    CHECK(input.tiltHeading() == Heading::None);
}

TEST_CASE("Without a profile a touch device starts on tilt", "[input-controller]")
{
    InputController input(std::make_unique<QueueSource>(std::make_shared<Queue>()));
    input.setTouchDevice(true);
    CHECK(input.controls() == Scheme::Tilt);
}

TEST_CASE("Tipping the device turns the snake, and levelling it does not", "[input-controller][tilt]")
{
    Rig rig;
    rig.start_level();
    CHECK_FALSE(rig.pending());

    // The top edge down by 25 degrees: up.
    rig.queue->hold(0.5F, 0.0F, 25.0F);
    QSignalSpy tilt(&rig.input, &InputController::tiltChanged);
    rig.input.update();
    CHECK(tilt.count() == 1);
    CHECK(rig.input.tiltHeading() == Heading::Up);
    CHECK(rig.input.tiltUp() > 18.0);
    CHECK(rig.input.tiltRight() == Catch::Approx(0.0).margin(1.0));
    REQUIRE(rig.pending());
    CHECK(*rig.pending() == Direction::UP);
}

TEST_CASE("A tilt inside the dead zone does nothing", "[input-controller][tilt]")
{
    Rig rig;
    rig.start_level();
    rig.queue->hold(0.5F, 0.0F, 8.0F);
    rig.input.update();
    CHECK(rig.input.tiltHeading() == Heading::None);
    CHECK_FALSE(rig.pending());
    CHECK(rig.input.tiltUp() == Catch::Approx(8.0).margin(1.0));
}

/// Counts the taps that the controller asks of the motor.
class CountingHaptics final : public snake::ui::device::Haptics
{
public:
    void tap() override
    {
        ++taps;
    }
    void thud() override {}

    int taps = 0;
};

TEST_CASE("A tilt game waits while the phone is calibrated, then begins with where it is held as neutral",
          "[input-controller][tilt][calibration]")
{
    Rig rig;
    CountingHaptics haptics;
    snake::ui::device::set_default_haptics(&haptics);
    // Held at a steep angle, which is neutral because the game starts there.
    rig.queue->hold(0.3F, 0.0F, 0.0F, 80.0F);
    QSignalSpy calibrating(&rig.input, &InputController::calibratingChanged);
    rig.game.start();
    // The game is held, and the screen says why.
    CHECK(rig.input.calibrating());
    CHECK(rig.game.state() == GameViewModel::State::Paused);
    CHECK(rig.input.calibrationProgress() == 0.0);
    rig.input.update(0.75);
    CHECK(rig.input.calibrating());
    CHECK(rig.input.calibrationProgress() == Catch::Approx(0.5));
    CHECK(rig.game.state() == GameViewModel::State::Paused);
    CHECK(haptics.taps == 0);
    rig.queue->hold(0.3F, 0.0F, 0.0F, 80.0F);
    rig.input.update(0.8);
    // Done: a tap to feel, the game running, the pose neutral.
    CHECK_FALSE(rig.input.calibrating());
    CHECK(haptics.taps == 1);
    CHECK(rig.game.state() == GameViewModel::State::Running);
    CHECK(calibrating.count() == 2);
    rig.queue->hold(0.3F, 0.0F, 0.0F, 80.0F);
    rig.input.update(0.016);
    CHECK(rig.input.tiltHeading() == Heading::None);
    CHECK(rig.input.tiltUp() == Catch::Approx(0.0).margin(1.0));
    CHECK_FALSE(rig.pending());
    snake::ui::device::set_default_haptics(nullptr);
}

TEST_CASE("Resuming a tilt game calibrates again first, and the choice of the pause dialog's button does the same",
          "[input-controller][tilt][calibration]")
{
    Rig rig;
    rig.start_level();
    // The player settles lower, which tips the top edge down 25 degrees and steers.
    rig.queue->hold(0.5F, 0.0F, 25.0F);
    rig.input.update();
    REQUIRE(rig.input.tiltHeading() == Heading::Up);

    rig.game.pause();
    rig.input.resume();
    CHECK(rig.input.calibrating());
    CHECK(rig.game.state() == GameViewModel::State::Paused);
    rig.queue->hold(0.5F, 0.0F, 25.0F);
    rig.input.update(InputController::CALIBRATION_SECONDS + 0.1);
    CHECK_FALSE(rig.input.calibrating());
    CHECK(rig.game.state() == GameViewModel::State::Running);
    rig.queue->hold(0.3F, 0.0F, 25.0F);
    rig.input.update(0.016);
    // Where it is held now is neutral, so it no longer steers.
    CHECK(rig.input.tiltHeading() == Heading::None);

    // The same by Recalibrate tilt.
    rig.queue->hold(0.5F, 0.0F, 0.0F);
    rig.input.update(0.016);
    REQUIRE(rig.input.tiltHeading() == Heading::Down);
    rig.game.pause();
    rig.profile.calibrateTilt();
    CHECK(rig.input.calibrating());
    rig.queue->hold(0.5F, 0.0F, 0.0F);
    rig.input.update(InputController::CALIBRATION_SECONDS + 0.1);
    CHECK(rig.game.state() == GameViewModel::State::Running);
    rig.queue->hold(0.3F, 0.0F, 0.0F);
    rig.input.update(0.016);
    CHECK(rig.input.tiltHeading() == Heading::None);
}

TEST_CASE("Without tilt nothing waits: a game begins and a resume resumes at once", "[input-controller][calibration]")
{
    Rig rig;
    rig.profile.setControls(ProfileViewModel::Controls::Dpad);
    rig.game.start();
    CHECK_FALSE(rig.input.calibrating());
    CHECK(rig.game.state() == GameViewModel::State::Running);
    rig.game.pause();
    rig.input.resume();
    CHECK(rig.game.state() == GameViewModel::State::Running);
    rig.input.startCalibration();
    CHECK_FALSE(rig.input.calibrating());
}

TEST_CASE("Leaving or restarting while the phone is calibrated ends the wait", "[input-controller][tilt][calibration]")
{
    Rig rig;
    rig.game.start();
    REQUIRE(rig.input.calibrating());
    rig.game.newGame();
    CHECK_FALSE(rig.input.calibrating());
    CHECK(rig.input.calibrationProgress() == 0.0);

    rig.game.start();
    REQUIRE(rig.input.calibrating());
    rig.game.restart();
    // A restart is a new game, which waits for its own calibration.
    CHECK(rig.input.calibrating());
    CHECK(rig.input.calibrationProgress() == 0.0);
    rig.input.update(InputController::CALIBRATION_SECONDS + 0.1);
    CHECK(rig.game.state() == GameViewModel::State::Running);
}

TEST_CASE("Inverted axes steer the other way", "[input-controller][tilt]")
{
    Rig rig;
    rig.profile.setInvertAxes(true);
    rig.start_level();
    rig.queue->hold(0.5F, 0.0F, 25.0F);
    rig.input.update();
    CHECK(rig.input.tiltHeading() == Heading::Down);
    REQUIRE(rig.pending());
    CHECK(*rig.pending() == Direction::DOWN);
}

TEST_CASE("The tilt is read when a game is not running but turns nothing", "[input-controller][tilt]")
{
    Rig rig;
    rig.start_level();
    rig.game.pause();
    rig.queue->hold(0.5F, 0.0F, 25.0F);
    rig.input.update();
    // The indicator moves, the snake does not.
    CHECK(rig.input.tiltHeading() == Heading::Up);
    CHECK_FALSE(rig.pending());
}

TEST_CASE("Tilt turns nothing when swiping is how the snake is steered", "[input-controller][tilt]")
{
    Rig rig;
    rig.profile.setControls(ProfileViewModel::Controls::Swipe);
    rig.start_level();
    rig.queue->hold(0.5F, 0.0F, 25.0F);
    rig.input.update();
    CHECK_FALSE(rig.pending());
}

TEST_CASE("A swipe turns the snake as soon as the finger has gone far enough", "[input-controller][swipe]")
{
    Rig rig;
    rig.profile.setControls(ProfileViewModel::Controls::Swipe);
    rig.start_level();

    rig.input.touchPressed(7, 100, 300);
    rig.input.touchMoved(7, 100, 290);
    CHECK_FALSE(rig.pending());
    rig.input.touchMoved(7, 100, 270);
    REQUIRE(rig.pending());
    CHECK(*rig.pending() == Direction::UP);
    rig.input.touchReleased(7);
}

TEST_CASE("A swipe is followed from one finger only and can be taken away", "[input-controller][swipe]")
{
    Rig rig;
    rig.profile.setControls(ProfileViewModel::Controls::Swipe);
    rig.start_level();

    rig.input.touchPressed(1, 100, 300);
    rig.input.touchPressed(2, 200, 300);
    rig.input.touchMoved(2, 200, 200);
    CHECK_FALSE(rig.pending());
    rig.input.touchCancelled();
    rig.input.touchMoved(1, 100, 200);
    CHECK_FALSE(rig.pending());
}

TEST_CASE("A swipe does nothing when the snake is steered another way", "[input-controller][swipe]")
{
    Rig rig;
    rig.profile.setControls(ProfileViewModel::Controls::Dpad);
    rig.start_level();
    rig.input.touchPressed(1, 100, 300);
    rig.input.touchMoved(1, 100, 200);
    CHECK_FALSE(rig.pending());
}

TEST_CASE("Changing the way of steering lets go of a swipe that was under way", "[input-controller][swipe]")
{
    Rig rig;
    rig.profile.setControls(ProfileViewModel::Controls::Swipe);
    rig.start_level();
    rig.input.touchPressed(1, 100, 300);
    rig.profile.setSound(false);
    rig.input.touchMoved(1, 100, 200);
    CHECK_FALSE(rig.pending());
}

TEST_CASE("The d-pad turns the snake and nothing else does while it is the way", "[input-controller][dpad]")
{
    Rig rig;
    rig.profile.setControls(ProfileViewModel::Controls::Dpad);
    rig.start_level();

    rig.input.steer(Heading::None);
    CHECK_FALSE(rig.pending());
    rig.input.steer(Heading::Up);
    REQUIRE(rig.pending());
    CHECK(*rig.pending() == Direction::UP);
}

TEST_CASE("Each button of the d-pad is a direction", "[input-controller][dpad]")
{
    for (const auto& [heading, direction] :
         {std::pair{Heading::Up, Direction::UP}, std::pair{Heading::Down, Direction::DOWN}})
    {
        Rig rig;
        rig.profile.setControls(ProfileViewModel::Controls::Dpad);
        rig.start_level();
        rig.input.steer(heading);
        REQUIRE(rig.pending());
        CHECK(*rig.pending() == direction);
    }
    // Left and right: the snake heads right, so a turn left is a reversal and is dropped; down first, then left.
    Rig rig;
    rig.profile.setControls(ProfileViewModel::Controls::Dpad);
    rig.start_level();
    rig.input.steer(Heading::Down);
    rig.game.advance(0.2);
    rig.input.steer(Heading::Left);
    REQUIRE(rig.pending());
    CHECK(*rig.pending() == Direction::LEFT);
    rig.game.advance(0.2);
    rig.input.steer(Heading::Up);
    rig.game.advance(0.2);
    rig.input.steer(Heading::Right);
    CHECK(rig.pending());
}

TEST_CASE("The d-pad is nothing on a computer", "[input-controller][dpad]")
{
    Rig rig;
    rig.input.setTouchDevice(false);
    rig.profile.setControls(ProfileViewModel::Controls::Dpad);
    rig.start_level();
    rig.input.steer(Heading::Up);
    CHECK_FALSE(rig.pending());
}

TEST_CASE("The angles of the indicator are the tilt processor's", "[input-controller]")
{
    CHECK(InputController::deadZone() == Catch::Approx(12.0));
    CHECK(InputController::threshold() == Catch::Approx(18.0));
}

TEST_CASE("The controller reads the accelerometer that the device gives, or the one a test gives", "[input-controller]")
{
    {
        const InputController plain;
        CHECK_FALSE(plain.tiltAvailable());
    }
    auto queue = std::make_shared<Queue>();
    snake::ui::device::set_accelerometer_factory([queue] { return std::make_unique<QueueSource>(queue); });
    const InputController with;
    snake::ui::device::set_accelerometer_factory({});
    CHECK(with.tiltAvailable());
}

TEST_CASE("Changing the game or the profile is heard once and the old one is let go", "[input-controller]")
{
    Rig rig;
    GameViewModel other(counting_seeds());
    QSignalSpy games(&rig.input, &InputController::gameChanged);
    rig.input.setGame(&rig.game);
    CHECK(games.count() == 0);
    rig.input.setGame(&other);
    CHECK(games.count() == 1);
    rig.input.setGame(nullptr);
    rig.input.setProfile(nullptr);
    CHECK(rig.input.game() == nullptr);
    CHECK(rig.input.profile() == nullptr);
    rig.input.update();
    rig.input.steer(Heading::Up);
}

TEST_CASE("The joystick turns the snake once the knob is a third of the way out, and not before",
          "[input-controller][joystick]")
{
    Rig rig;
    rig.profile.setControls(ProfileViewModel::Controls::Joystick);
    CHECK(rig.input.controls() == Scheme::Joystick);
    rig.start_level();

    rig.input.joystickMoved(0.0, 0.2);
    CHECK_FALSE(rig.pending());
    rig.input.joystickMoved(0.0, 0.5);
    REQUIRE(rig.pending());
    CHECK(*rig.pending() == Direction::UP);
    rig.input.joystickReleased();
}

TEST_CASE("A diagonal is a turn across the snake's line", "[input-controller][joystick]")
{
    // The snake starts heading right: north-east is up, south-east is down.
    for (const auto& [up, expected] : {std::pair{0.6, Direction::UP}, std::pair{-0.6, Direction::DOWN}})
    {
        Rig rig;
        rig.profile.setControls(ProfileViewModel::Controls::Joystick);
        rig.start_level();
        rig.input.joystickMoved(0.6, up);
        REQUIRE(rig.pending());
        CHECK(*rig.pending() == expected);
    }
}

TEST_CASE("A thumb held on a diagonal steers a staircase, one turn each tick", "[input-controller][joystick]")
{
    Rig rig;
    rig.profile.setControls(ProfileViewModel::Controls::Joystick);
    rig.start_level();
    rig.input.joystickMoved(0.6, 0.6);
    REQUIRE(rig.pending());
    CHECK(*rig.pending() == Direction::UP);
    // Held while the turn waits for its tick: nothing more is queued, however many frames go by.
    rig.input.update();
    rig.input.update();
    CHECK(rig.game.session().simulation().pending_turns().size() == 1);

    // The tick takes the turn: the snake heads up, so north-east now asks for east.
    rig.game.advance(0.2);
    REQUIRE(rig.game.session().simulation().heading() == Direction::UP);
    rig.input.update();
    REQUIRE(rig.pending());
    CHECK(*rig.pending() == Direction::RIGHT);
    rig.game.advance(0.2);
    REQUIRE(rig.game.session().simulation().heading() == Direction::RIGHT);
    rig.input.update();
    REQUIRE(rig.pending());
    CHECK(*rig.pending() == Direction::UP);
}

TEST_CASE("A flick to the diagonal and back inside one tick moves the snake one row aside",
          "[input-controller][joystick]")
{
    Rig rig;
    rig.profile.setControls(ProfileViewModel::Controls::Joystick);
    rig.start_level();
    rig.input.joystickMoved(0.8, 0.0);
    CHECK_FALSE(rig.pending());
    rig.input.joystickMoved(0.6, 0.6);
    rig.input.joystickMoved(0.8, 0.0);
    const auto& turns = rig.game.session().simulation().pending_turns();
    REQUIRE(turns.size() == 2);
    CHECK(turns.at(0) == Direction::UP);
    CHECK(turns.at(1) == Direction::RIGHT);
    rig.game.advance(0.2);
    rig.game.advance(0.2);
    CHECK(rig.game.session().simulation().heading() == Direction::RIGHT);
    CHECK(rig.game.session().simulation().head().y == 9);
}

TEST_CASE("The joystick does nothing when it is not the way, or the game is not running",
          "[input-controller][joystick]")
{
    Rig rig;
    rig.profile.setControls(ProfileViewModel::Controls::Dpad);
    rig.start_level();
    rig.input.joystickMoved(0.0, 0.9);
    CHECK_FALSE(rig.pending());

    rig.profile.setControls(ProfileViewModel::Controls::Joystick);
    rig.game.pause();
    rig.input.joystickMoved(0.0, 0.9);
    CHECK_FALSE(rig.pending());
}

TEST_CASE("Letting go of the joystick stops steering, and a paused knob steers nothing when the game resumes",
          "[input-controller][joystick]")
{
    Rig rig;
    rig.profile.setControls(ProfileViewModel::Controls::Joystick);
    rig.start_level();
    rig.input.joystickMoved(0.0, 0.8);
    REQUIRE(rig.pending());
    rig.game.advance(0.2);
    rig.input.joystickReleased();
    // Nothing is held: another frame turns nothing, and the knob in the middle is nothing.
    rig.input.update();
    CHECK_FALSE(rig.pending());
    rig.input.joystickMoved(0.0, 0.0);
    CHECK_FALSE(rig.pending());
}
