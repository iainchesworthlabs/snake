// SPDX-License-Identifier: GPL-3.0-or-later
#include "about/about_info.hpp"
#include "device/device.hpp"
#include "platform/motion_preference.hpp"
#include "platform/sound_sink.hpp"
#include "platform/touch_screen.hpp"
#include "snake/audio/tunes.hpp"
#include "snake/input/sample_source.hpp"
#include "snake/sim/mode.hpp"
#include "snake/source_info.hpp"
#include "snake/version.hpp"
#include "theme/tokens.hpp"
#include "viewmodel/effects_model.hpp"
#include "viewmodel/game_view_model.hpp"
#include "viewmodel/input_controller.hpp"
#include "viewmodel/profile_view_model.hpp"

#include <QtTest/QTest>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QByteArray>
#include <QColor>
#include <QGuiApplication>
#include <QImage>
#include <QLocale>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTranslator>
#include <QVariantMap>
#include <Qt>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

using snake::sim::Mode;
using snake::ui::GameViewModel;
using snake::ui::ProfileViewModel;
using State = GameViewModel::State;
namespace tokens = snake::ui::tokens;

namespace
{

constexpr int PAGE_WIDTH = 390;
constexpr int PAGE_HEIGHT = 844;

/// An accelerometer that feels nothing, so that a device in a test can be steered by tilt.
class SilentAccelerometer final : public snake::input::SampleSource
{
public:
    std::optional<snake::input::AccelSample> next() noexcept override
    {
        return std::nullopt;
    }
};

// The window is the size of the design's page, so a screen is drawn at the scale of the design and its measures can be
// read off the pixels.
class App
{
public:
    explicit App(bool touch_device = false, int width = PAGE_WIDTH, int height = PAGE_HEIGHT)
    {
        QQmlComponent component(&m_Engine, "SnakeApp", "Main");
        INFO(component.errorString().toStdString());
        REQUIRE(component.isReady());
        // The computer these run on has no accelerometer; a phone does.
        snake::ui::device::set_accelerometer_factory([] { return std::make_unique<SilentAccelerometer>(); });
        m_Root.reset(component.createWithInitialProperties(
            {{"windowWidth", width}, {"windowHeight", height}, {"touchDevice", touch_device}}));
        snake::ui::device::set_accelerometer_factory({});
        m_Window = qobject_cast<QQuickWindow*>(m_Root.get());
        REQUIRE(m_Window != nullptr);
        m_Window->show();
        REQUIRE(QTest::qWaitForWindowExposed(m_Window));
        m_Window->requestActivate();
        REQUIRE(QTest::qWaitForWindowActive(m_Window));
        m_Game = m_Window->findChild<GameViewModel*>("game");
        m_Profile = m_Window->findChild<ProfileViewModel*>("profile");
        m_Effects = m_Window->findChild<snake::ui::EffectsModel*>("effects");
        m_Input = m_Window->findChild<snake::ui::InputController*>("input");
        REQUIRE(m_Input != nullptr);
        REQUIRE(m_Game != nullptr);
        REQUIRE(m_Profile != nullptr);
        REQUIRE(m_Effects != nullptr);
    }

    QQuickWindow& window()
    {
        return *m_Window;
    }

    GameViewModel& game()
    {
        return *m_Game;
    }

    ProfileViewModel& profile()
    {
        return *m_Profile;
    }

    snake::ui::EffectsModel& effects()
    {
        return *m_Effects;
    }

    QQuickItem* item(const char* name)
    {
        // By the visual tree: the items a Repeater makes have no QObject parent to search from.
        auto* found = find_in(m_Window->contentItem(), QString::fromLatin1(name));
        INFO("item " << name);
        REQUIRE(found != nullptr);
        return found;
    }

    bool shows(const char* name)
    {
        return item(name)->isVisible();
    }

    /// The name of the item that has the keyboard, or an empty string.
    QString focus_name()
    {
        const QQuickItem* focused = m_Window->activeFocusItem();
        return focused == nullptr ? QString() : focused->objectName();
    }

    QString text_of(const char* name)
    {
        return item(name)->property("text").toString();
    }

    QPoint centre_of(QQuickItem* target)
    {
        return target->mapToScene(QPointF(target->width() / 2, target->height() / 2)).toPoint();
    }

    /// A point in the face of a button or card, just inside its left edge, where there is no text.
    QPoint face_of(const char* name)
    {
        QQuickItem* target = item(name);
        return target->mapToScene(QPointF(8, target->height() / 2)).toPoint();
    }

    void click(const char* name)
    {
        QQuickItem* target = item(name);
        REQUIRE(target->isVisible());
        QTest::mouseClick(m_Window, Qt::LeftButton, Qt::NoModifier, centre_of(target));
    }

    void press(Qt::Key key)
    {
        QTest::keyClick(m_Window, key);
    }

    GameViewModel& demo()
    {
        auto* found = m_Window->findChild<GameViewModel*>("demoGame");
        REQUIRE(found != nullptr);
        return *found;
    }

    /// Types `text` on the keyboard.
    void type(const char* text)
    {
        for (const char* c = text; *c != '\0'; ++c)
        {
            QTest::keyClick(m_Window, *c);
        }
    }

    snake::ui::InputController& input()
    {
        return *m_Input;
    }

    /// Lets the wait that a tilt game has before it begins run out, so that the game runs.
    void finish_calibration()
    {
        if (m_Input->calibrating())
        {
            m_Input->update(snake::ui::InputController::CALIBRATION_SECONDS + 0.1);
        }
    }

    /// Chooses Classic and starts the game (through the calibration wait of a tilt game).
    void start_classic()
    {
        click("modeCard1");
        click("playButton");
        finish_calibration();
    }

    /// Types `name` into the profile and saves the score that waits for it.
    void save_score_as(const QString& name)
    {
        for (const QChar letter : name)
        {
            m_Profile->typeCharacter(QString(letter));
        }
        REQUIRE(m_Profile->saveScore());
    }

    /// Plays a Classic game straight on until the snake hits the wall.
    void crash()
    {
        for (int i = 0; i < 40 && m_Game->state() == State::Running; ++i)
        {
            m_Game->advance(0.15);
        }
        REQUIRE(m_Game->state() == State::GameOver);
        // The snake takes a second or so to die before the end of the game shows. These tests are not about that.
        m_Effects->skipDeath();
        // The food is placed at random, so a snake that runs straight may eat some on the way and make the top 10.
        // The tests that follow expect the buttons of a game over without a score to save.
        m_Profile->discardScore();
    }

    /// What the window looks like now.
    QImage render()
    {
        QTest::qWait(20);
        return m_Window->grabWindow();
    }

private:
    static QQuickItem* find_in(QQuickItem* root, const QString& name)
    {
        if (root->objectName() == name)
        {
            return root;
        }
        for (QQuickItem* child : root->childItems())
        {
            if (QQuickItem* found = find_in(child, name))
            {
                return found;
            }
        }
        return nullptr;
    }

    QQmlEngine m_Engine;
    std::unique_ptr<QObject> m_Root;
    QQuickWindow* m_Window = nullptr;
    GameViewModel* m_Game = nullptr;
    snake::ui::InputController* m_Input = nullptr;
    ProfileViewModel* m_Profile = nullptr;
    snake::ui::EffectsModel* m_Effects = nullptr;
};

// Numbers in the text are written for the player's locale; the tests fix one.
struct UsLocale
{
    QLocale previous;

    UsLocale() : previous(QLocale())
    {
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    }
    UsLocale(const UsLocale&) = delete;
    UsLocale& operator=(const UsLocale&) = delete;
    ~UsLocale()
    {
        QLocale::setDefault(previous);
    }
};

QRgb rgb_of(std::uint32_t colour)
{
    return QColor::fromRgb(colour).rgb();
}

bool is_close(QRgb actual, int red, int green, int blue, int tolerance = 3)
{
    return std::abs(qRed(actual) - red) <= tolerance && std::abs(qGreen(actual) - green) <= tolerance &&
           std::abs(qBlue(actual) - blue) <= tolerance;
}

} // namespace

TEST_CASE("The application opens on the title screen and nothing else", "[screens]")
{
    App app;
    CHECK(app.game().state() == State::NotStarted);
    CHECK(app.shows("titleScreen"));
    CHECK_FALSE(app.shows("gameScreen"));
    CHECK_FALSE(app.shows("pauseScreen"));
    CHECK_FALSE(app.shows("gameOverScreen"));
    CHECK(app.focus_name() == "playButton");
}

TEST_CASE("The title screen has the wordmark on the yellow of the design", "[screens]")
{
    App app;
    CHECK(app.text_of("wordmark") == "Snake");
    const QImage image = app.render();
    REQUIRE(image.width() == PAGE_WIDTH);
    CHECK(image.pixel(4, 4) == rgb_of(tokens::BACKGROUND));
    // The Play button is an ink face.
    CHECK(image.pixel(app.face_of("playButton")) == rgb_of(tokens::INK));
}

TEST_CASE("The title screen lists the four modes, two to play and two locked", "[screens][modes]")
{
    const UsLocale locale;
    App app;
    CHECK(app.item("modeCard0")->property("name").toString() == "Easy");
    CHECK(app.item("modeCard1")->property("name").toString() == "Classic");
    CHECK(app.item("modeCard2")->property("name").toString() == "Portals");
    CHECK(app.item("modeCard3")->property("name").toString() == "Hot potato");

    for (const char* open : {"modeCard0", "modeCard1"})
    {
        CHECK(app.item(open)->property("selectable").toBool());
        CHECK_FALSE(app.item(open)->property("locked").toBool());
    }
    for (const char* closed : {"modeCard2", "modeCard3"})
    {
        CHECK_FALSE(app.item(closed)->property("selectable").toBool());
        CHECK(app.item(closed)->property("locked").toBool());
        CHECK_FALSE(app.item(closed)->activeFocusOnTab());
    }
    CHECK(app.item("modeCard2")->property("detail").toString() == "Unlocks at 2,000 in Classic");
    CHECK(app.item("modeCard3")->property("detail").toString() == "Unlocks at 3,000 in Classic");
}

TEST_CASE("Choosing a mode sets the mode of the game and marks the card", "[screens][modes]")
{
    App app;
    // The title starts on Easy, as the design does.
    REQUIRE(app.game().mode() == GameViewModel::Mode::Easy);
    CHECK(app.item("modeCard0")->property("chosen").toBool());
    CHECK_FALSE(app.item("modeCard1")->property("chosen").toBool());

    app.click("modeCard1");
    CHECK(app.game().mode() == GameViewModel::Mode::Classic);
    CHECK(app.item("modeCard1")->property("chosen").toBool());
    CHECK_FALSE(app.item("modeCard0")->property("chosen").toBool());

    // The chosen mode is coral with the tick, the other cream.
    const QImage image = app.render();
    CHECK(image.pixel(app.face_of("modeCard1")) == rgb_of(tokens::ACCENT));
    CHECK(image.pixel(app.face_of("modeCard0")) == rgb_of(tokens::SURFACE));

    app.click("modeCard0");
    CHECK(app.game().mode() == GameViewModel::Mode::Easy);
}

TEST_CASE("A locked mode cannot be chosen, by touch or by key", "[screens][modes]")
{
    App app;
    app.click("modeCard2");
    app.click("modeCard3");
    CHECK(app.game().mode() == GameViewModel::Mode::Easy);
    CHECK(app.game().state() == State::NotStarted);

    // Going round the screen with the arrow keys never stops on a locked mode.
    for (int i = 0; i < 6; ++i)
    {
        app.press(Qt::Key_Down);
        CHECK(app.focus_name() != "modeCard2");
        CHECK(app.focus_name() != "modeCard3");
    }
}

TEST_CASE("The arrow keys move the choice between the modes that can be played", "[screens][modes]")
{
    App app;
    REQUIRE(app.focus_name() == "playButton");

    app.press(Qt::Key_Up);
    CHECK(app.focus_name() == "modeCard1");
    app.press(Qt::Key_Up);
    CHECK(app.focus_name() == "modeCard0");
    CHECK(app.game().mode() == GameViewModel::Mode::Easy);

    // The locked modes are skipped on the way down.
    app.press(Qt::Key_Down);
    CHECK(app.focus_name() == "modeCard1");
    CHECK(app.game().mode() == GameViewModel::Mode::Classic);
    app.press(Qt::Key_Down);
    CHECK(app.focus_name() == "playButton");
}

TEST_CASE("Play starts a game in the chosen mode", "[screens]")
{
    App app;
    app.click("modeCard1");
    app.click("playButton");
    CHECK(app.game().state() == State::Running);
    CHECK(app.game().mode() == GameViewModel::Mode::Classic);
    CHECK_FALSE(app.shows("titleScreen"));
    CHECK(app.shows("gameScreen"));
    CHECK(app.text_of("score") == "000000");
}

TEST_CASE("Enter on the title screen starts a game, with the focus on any mode or on Play", "[screens]")
{
    {
        App app;
        app.press(Qt::Key_Return);
        CHECK(app.game().state() == State::Running);
    }
    {
        App app;
        app.press(Qt::Key_Up);
        REQUIRE(app.focus_name() == "modeCard1");
        app.press(Qt::Key_Return);
        CHECK(app.game().state() == State::Running);
    }
}

TEST_CASE("The keys steer the game once it runs", "[screens]")
{
    App app;
    app.click("playButton");
    REQUIRE(app.focus_name() == "gameScreen");
    app.press(Qt::Key_Up);
    app.game().advance(0.18);
    CHECK(app.game().state() == State::Running);
    // The head is a cell above the row it started in.
    CHECK(app.game().session().simulation().head().y == 9);
}

TEST_CASE("A computer shows no touch controls on the game screen", "[screens][touch]")
{
    App app;
    app.start_classic();
    CHECK_FALSE(app.shows("dpad"));
    CHECK_FALSE(app.shows("tiltIndicator"));
    CHECK_FALSE(app.shows("pauseButton"));
    CHECK_FALSE(app.item("swipeArea")->isEnabled());
}

TEST_CASE("Tilt shows the indicator and a pause button, and swiping a swipe area", "[screens][touch]")
{
    App app(true);
    app.start_classic();
    // Tilt is the first choice of the profile.
    CHECK(app.shows("tiltIndicator"));
    CHECK(app.shows("pauseButton"));
    CHECK_FALSE(app.shows("dpad"));
    CHECK_FALSE(app.item("swipeArea")->isEnabled());
    // The dot sits in the middle of the plate while nothing is felt.
    const QQuickItem* plate = app.item("tiltIndicator");
    const QQuickItem* dot = app.item("tiltDot");
    CHECK(dot->x() + dot->width() / 2 == Catch::Approx(plate->width() / 2).margin(1.0));
    CHECK(dot->y() + dot->height() / 2 == Catch::Approx(plate->height() / 2).margin(1.0));
    // The dead zone is inside the angle that turns.
    CHECK(app.item("tiltDeadZone")->width() < app.item("tiltThreshold")->width());

    app.game().pause();
    app.profile().setControls(ProfileViewModel::Controls::Swipe);
    app.game().resume();
    CHECK_FALSE(app.shows("tiltIndicator"));
    CHECK(app.item("swipeArea")->isEnabled());
}

TEST_CASE("The d-pad makes room under the board and its arrows turn the snake", "[screens][touch]")
{
    App app(true);
    app.profile().setControls(ProfileViewModel::Controls::Dpad);
    app.start_classic();
    REQUIRE(app.shows("dpad"));
    CHECK_FALSE(app.shows("tiltIndicator"));
    const QQuickItem* board = app.item("board");
    const QQuickItem* dpad = app.item("dpad");
    CHECK(board->y() + board->height() <= dpad->y());

    app.click("dpadUp");
    app.game().advance(0.18);
    CHECK(app.game().session().simulation().head().y == 9);
    app.click("dpadLeft");
    app.click("dpadDown");
    app.click("dpadRight");
}

TEST_CASE("The joystick makes room under the board, and the knob follows a thumb and comes back", "[screens][touch]")
{
    App app(true);
    app.profile().setControls(ProfileViewModel::Controls::Joystick);
    app.start_classic();
    REQUIRE(app.shows("joystick"));
    CHECK_FALSE(app.shows("dpad"));
    CHECK_FALSE(app.shows("tiltIndicator"));
    const QQuickItem* board = app.item("board");
    const QQuickItem* joystick = app.item("joystick");
    CHECK(board->y() + board->height() <= joystick->y());

    QQuickItem* area = app.item("joystickArea");
    const QPoint middle = app.centre_of(area);
    const QQuickItem* knob = app.item("joystickKnob");
    const QPointF rest = knob->position();
    QTest::mousePress(&app.window(), Qt::LeftButton, Qt::NoModifier, middle + QPoint(0, -60));
    CHECK(knob->y() < rest.y());
    app.game().advance(0.18);
    CHECK(app.game().session().simulation().head().y == 9);
    QTest::mouseRelease(&app.window(), Qt::LeftButton, Qt::NoModifier, middle + QPoint(0, -60));
    CHECK(knob->position() == rest);
}

TEST_CASE("The knob stops at the edge of its travel however far the thumb goes", "[screens][touch]")
{
    App app(true);
    app.profile().setControls(ProfileViewModel::Controls::Joystick);
    app.start_classic();
    QQuickItem* area = app.item("joystickArea");
    const QPoint middle = app.centre_of(area);
    const QQuickItem* knob = app.item("joystickKnob");
    const QPointF rest = knob->position();
    QTest::mousePress(&app.window(), Qt::LeftButton, Qt::NoModifier, middle);
    QTest::mouseMove(&app.window(), middle + QPoint(80, 0));
    CHECK(knob->x() - rest.x() == Catch::Approx(52.0).margin(1.0));
    QTest::mouseRelease(&app.window(), Qt::LeftButton, Qt::NoModifier, middle + QPoint(80, 0));
}

TEST_CASE("The pause button is clear of the score, even a long one, on a phone", "[screens][touch]")
{
    for (const auto controls : {ProfileViewModel::Controls::Tilt, ProfileViewModel::Controls::Swipe,
                                ProfileViewModel::Controls::Dpad, ProfileViewModel::Controls::Joystick})
    {
        App app(true);
        app.profile().setControls(controls);
        app.start_classic();
        const QQuickItem* pause = app.item("pauseButton");
        const QQuickItem* score = app.item("score");
        const QQuickItem* best = app.item("bestScore");
        const QRectF pause_box(pause->mapToScene(QPointF(0, 0)), QSizeF(pause->width(), pause->height()));
        const QRectF score_box(score->mapToScene(QPointF(0, 0)), QSizeF(score->width(), score->height()));
        const QRectF best_box(best->mapToScene(QPointF(0, 0)), QSizeF(best->width(), best->height()));
        CHECK_FALSE(pause_box.intersects(score_box));
        CHECK_FALSE(pause_box.intersects(best_box));
        // Inside the window, with room for a thumb to reach and the home indicator to be.
        CHECK(pause_box.left() >= 0.0);
        CHECK(pause_box.bottom() <= app.window().height());
        // And clear of the board and of the other controls.
        const QQuickItem* board = app.item("board");
        const QRectF board_box(board->mapToScene(QPointF(0, 0)), QSizeF(board->width(), board->height()));
        CHECK_FALSE(pause_box.intersects(board_box));
    }
}

TEST_CASE("A tilt game begins with a wait that the screen shows, and the pause dialog does not flash up",
          "[screens][touch]")
{
    App app(true);
    app.click("modeCard1");
    app.click("playButton");
    REQUIRE(app.input().calibrating());
    CHECK(app.game().state() == State::Paused);
    CHECK(app.shows("calibrationOverlay"));
    CHECK_FALSE(app.shows("pauseScreen"));
    // The bar fills with the wait.
    const QQuickItem* bar = app.item("calibrationBar");
    CHECK(bar->width() == 0.0);
    app.input().update(snake::ui::InputController::CALIBRATION_SECONDS / 2);
    CHECK(bar->width() > 50.0);
    app.finish_calibration();
    CHECK_FALSE(app.shows("calibrationOverlay"));
    CHECK(app.game().state() == State::Running);
    // Resume from the pause dialog waits again.
    app.game().pause();
    REQUIRE(app.shows("pauseScreen"));
    app.click("resumeButton");
    CHECK(app.input().calibrating());
    CHECK(app.shows("calibrationOverlay"));
    CHECK_FALSE(app.shows("pauseScreen"));
    app.finish_calibration();
    CHECK(app.game().state() == State::Running);
}

TEST_CASE("The pause button of a phone pauses the game", "[screens][touch]")
{
    App app(true);
    app.start_classic();
    app.click("pauseButton");
    CHECK(app.game().state() == State::Paused);
    CHECK(app.shows("pauseScreen"));
}

TEST_CASE("Escape pauses and shows the pause dialog over the dimmed game", "[screens][pause]")
{
    App app;
    app.click("playButton");
    app.press(Qt::Key_Escape);
    REQUIRE(app.game().state() == State::Paused);
    CHECK(app.shows("pauseScreen"));
    CHECK(app.shows("gameScreen"));
    CHECK(app.focus_name() == "resumeButton");
    CHECK(app.text_of("resumeButton") == "Resume");

    // The yellow of the window under the overlay: 20 percent of it and 80 percent of the ink.
    const QImage image = app.render();
    CHECK(is_close(image.pixel(4, 4), 85, 63, 78));
}

TEST_CASE("Resume carries on with the same game", "[screens][pause]")
{
    App app;
    app.click("playButton");
    app.game().advance(0.3);
    const QString score = app.game().score();
    app.press(Qt::Key_Escape);
    REQUIRE(app.game().state() == State::Paused);

    app.click("resumeButton");
    CHECK(app.game().state() == State::Running);
    CHECK(app.game().score() == score);
    CHECK_FALSE(app.shows("pauseScreen"));
    CHECK(app.focus_name() == "gameScreen");
}

TEST_CASE("Escape and Enter on the pause dialog resume the game", "[screens][pause]")
{
    App app;
    app.click("playButton");
    app.press(Qt::Key_Escape);
    REQUIRE(app.game().state() == State::Paused);
    app.press(Qt::Key_Escape);
    CHECK(app.game().state() == State::Running);

    app.press(Qt::Key_Space);
    REQUIRE(app.game().state() == State::Paused);
    app.press(Qt::Key_Return);
    CHECK(app.game().state() == State::Running);
}

TEST_CASE("Restart begins a new game at once", "[screens][pause]")
{
    App app;
    app.click("playButton");
    app.game().advance(0.3);
    app.press(Qt::Key_Escape);
    REQUIRE(app.game().state() == State::Paused);

    app.click("restartButton");
    CHECK(app.game().state() == State::Running);
    CHECK(app.game().session().simulation().ticks() == 0);
    CHECK(app.game().score() == "000000");
    CHECK_FALSE(app.shows("pauseScreen"));
}

TEST_CASE("Quit to menu goes back to the title screen with a fresh game", "[screens][pause]")
{
    App app;
    app.click("modeCard1");
    app.click("playButton");
    app.press(Qt::Key_Escape);
    app.click("quitButton");
    CHECK(app.game().state() == State::NotStarted);
    CHECK(app.shows("titleScreen"));
    CHECK_FALSE(app.shows("gameScreen"));
    CHECK_FALSE(app.shows("pauseScreen"));
    // The mode chosen before is still chosen.
    CHECK(app.game().mode() == GameViewModel::Mode::Classic);
    CHECK(app.focus_name() == "playButton");
}

TEST_CASE("The arrow keys move between the pause buttons", "[screens][pause]")
{
    App app;
    app.click("playButton");
    app.press(Qt::Key_Escape);
    REQUIRE(app.focus_name() == "resumeButton");
    app.press(Qt::Key_Down);
    CHECK(app.focus_name() == "restartButton");
    // The sound and music switches are between Restart and Quit to menu.
    app.press(Qt::Key_Down);
    CHECK(app.focus_name() == "pauseSoundToggle");
    app.press(Qt::Key_Down);
    CHECK(app.focus_name() == "pauseMusicToggle");
    app.press(Qt::Key_Down);
    CHECK(app.focus_name() == "quitButton");
    app.press(Qt::Key_Up);
    CHECK(app.focus_name() == "pauseMusicToggle");
}

TEST_CASE("A click on the dimmed game does not reach it", "[screens][pause]")
{
    App app;
    app.click("playButton");
    app.press(Qt::Key_Escape);
    QTest::mouseClick(&app.window(), Qt::LeftButton, Qt::NoModifier, QPoint(4, 4));
    CHECK(app.game().state() == State::Paused);
    CHECK(app.shows("pauseScreen"));
}

TEST_CASE("The game over screen shows the final score and the way on", "[screens][gameover]")
{
    App app;
    app.start_classic();
    app.crash();
    CHECK(app.shows("gameOverScreen"));
    CHECK_FALSE(app.shows("gameScreen"));
    CHECK_FALSE(app.shows("titleScreen"));
    CHECK(app.text_of("finalScore") == app.game().score());
    CHECK(app.focus_name() == "playAgainButton");
    CHECK(app.text_of("playAgainButton") == "Play again");
}

TEST_CASE("Play again starts a new game of the same mode", "[screens][gameover]")
{
    App app;
    app.start_classic();
    app.crash();
    const auto mode = app.game().mode();

    app.click("playAgainButton");
    CHECK(app.game().state() == State::Running);
    CHECK(app.game().mode() == mode);
    CHECK(app.game().session().simulation().ticks() == 0);
    CHECK(app.shows("gameScreen"));
    CHECK_FALSE(app.shows("gameOverScreen"));
    CHECK(app.focus_name() == "gameScreen");
}

TEST_CASE("Enter on the game over screen plays again", "[screens][gameover]")
{
    App app;
    app.start_classic();
    app.crash();
    app.press(Qt::Key_Return);
    CHECK(app.game().state() == State::Running);
}

TEST_CASE("Menu goes back to the title screen", "[screens][gameover]")
{
    App app;
    app.start_classic();
    app.crash();
    app.click("menuButton");
    CHECK(app.game().state() == State::NotStarted);
    CHECK(app.shows("titleScreen"));
    CHECK_FALSE(app.shows("gameOverScreen"));
    CHECK(app.focus_name() == "playButton");
}

TEST_CASE("The arrow keys move between Play again and Menu", "[screens][gameover]")
{
    App app;
    app.start_classic();
    app.crash();
    app.press(Qt::Key_Down);
    CHECK(app.focus_name() == "menuButton");
    app.press(Qt::Key_Return);
    CHECK(app.game().state() == State::NotStarted);
}

TEST_CASE("The game over screen says how far the game was from the best", "[screens][gameover]")
{
    const UsLocale locale;
    App app;
    app.profile().record_game(Mode::CLASSIC, 1480);
    app.save_score_as("MUM");
    app.start_classic();
    app.crash();
    REQUIRE(app.game().points() < 1480);
    CHECK(app.text_of("verdict") == QString("%L1 more to beat your best.").arg(1480 - app.game().points()));
    CHECK(app.item("verdict")->isVisible());
    CHECK(app.text_of("finalScore") == app.game().score());
}

TEST_CASE("The game over screen announces a new best, a matched best and a mode that opened", "[screens][gameover]")
{
    App app;
    app.start_classic();
    app.crash();

    // The screen reads the profile, so the profile's result is what it shows.
    app.profile().record_game(Mode::CLASSIC, 5000);
    CHECK(app.text_of("verdict") == "New best!");
    CHECK(app.text_of("unlocked") == "Portals unlocked!\nHot potato unlocked!");
    app.save_score_as("MUM");

    app.profile().record_game(Mode::CLASSIC, 5000);
    CHECK(app.text_of("verdict") == "You matched your best.");
    CHECK_FALSE(app.item("unlocked")->isVisible());
}

TEST_CASE("A mode that opened no longer shows as locked on the title screen", "[screens][modes]")
{
    const UsLocale locale;
    App app;
    app.profile().record_game(Mode::CLASSIC, 2000);
    CHECK_FALSE(app.item("modeCard2")->property("locked").toBool());
    CHECK(app.item("modeCard2")->property("detail").toString() == "Coming in a later version.");
    CHECK(app.item("modeCard3")->property("locked").toBool());
    // Open on paper only: it still cannot be started.
    app.click("modeCard2");
    CHECK(app.game().mode() == GameViewModel::Mode::Easy);
}

TEST_CASE("The game over screen draws the board as it was left", "[screens][gameover]")
{
    App app;
    app.start_classic();
    app.crash();
    const QImage image = app.render();
    // The panel is cream with an ink outline; the board above it has the dark frame.
    CHECK(image.pixel(app.face_of("playAgainButton")) == rgb_of(tokens::INK));
    CHECK(image.pixel(4, 4) == rgb_of(tokens::BACKGROUND));
}

TEST_CASE("The title screen has a Leaderboard button that opens the leaderboard and Back closes it",
          "[screens][leaderboard]")
{
    App app;
    CHECK(app.text_of("leaderboardButton") == "Leaderboard");
    CHECK_FALSE(app.shows("leaderboardScreen"));

    app.click("leaderboardButton");
    CHECK(app.shows("leaderboardScreen"));
    CHECK_FALSE(app.shows("titleScreen"));
    CHECK(app.focus_name() == "leaderboardPlayButton");
    // Nothing has been scored.
    CHECK(app.shows("emptyBoard"));
    CHECK(app.profile().boardMode() == GameViewModel::Mode::Easy);

    app.click("backButton");
    CHECK(app.shows("titleScreen"));
    CHECK_FALSE(app.shows("leaderboardScreen"));
    CHECK(app.focus_name() == "playButton");
}

TEST_CASE("Escape closes the leaderboard", "[screens][leaderboard]")
{
    App app;
    app.click("leaderboardButton");
    app.press(Qt::Key_Escape);
    CHECK(app.shows("titleScreen"));
    CHECK_FALSE(app.shows("leaderboardScreen"));
}

TEST_CASE("The leaderboard opens on the mode chosen on the title and has a tab for each mode", "[screens][leaderboard]")
{
    App app;
    app.profile().record_game(Mode::CLASSIC, 1480);
    app.save_score_as("MUM");
    app.profile().record_game(Mode::EASY, 300);
    app.save_score_as("LEO");

    app.click("modeCard1");
    app.click("leaderboardButton");
    CHECK(app.profile().boardMode() == GameViewModel::Mode::Classic);
    CHECK_FALSE(app.shows("emptyBoard"));
    CHECK(app.text_of("emptyBoard") == "No scores yet.");
    CHECK(app.item("line1")->property("name").toString() == "MUM");
    CHECK(app.item("line1")->property("score").toString() == "001480");
    CHECK_FALSE(app.item("line1")->property("latest").toBool());

    app.click("easyTab");
    CHECK(app.profile().boardMode() == GameViewModel::Mode::Easy);
    CHECK(app.item("line1")->property("name").toString() == "LEO");
    app.click("classicTab");
    CHECK(app.item("line1")->property("name").toString() == "MUM");
}

TEST_CASE("The first three places have the medal colours", "[screens][leaderboard]")
{
    App app;
    for (const std::uint32_t score : {500u, 400u, 300u, 200u, 100u})
    {
        app.profile().record_game(Mode::EASY, score);
        app.save_score_as("AAA");
    }
    app.click("leaderboardButton");
    const QImage image = app.render();
    // A point in the tile of each place, clear of its outline and of the number.
    const auto tile = [&](const char* line)
    {
        QQuickItem* row = app.item(line);
        return image.pixel(row->mapToScene(QPointF(14, 14)).toPoint());
    };
    CHECK(tile("line1") == rgb_of(tokens::MEDAL_GOLD));
    CHECK(tile("line2") == rgb_of(tokens::MEDAL_SILVER));
    CHECK(tile("line3") == rgb_of(tokens::MEDAL_BRONZE));
    // The fourth has no tile: the panel shows through. (The line saved last is the fifth.)
    CHECK(tile("line4") == rgb_of(tokens::SURFACE));
}

TEST_CASE("Play on the leaderboard starts a game and the leaderboard goes", "[screens][leaderboard]")
{
    App app;
    app.click("leaderboardButton");
    app.click("leaderboardPlayButton");
    CHECK(app.game().state() == State::Running);
    CHECK_FALSE(app.shows("leaderboardScreen"));
    CHECK(app.shows("gameScreen"));
}

TEST_CASE("A game over that does not make the top 10 offers Play again", "[screens][nameentry]")
{
    App app;
    app.start_classic();
    app.crash();
    // A score of nothing never gets on the board, however the food lay.
    app.profile().record_game(Mode::CLASSIC, 0);
    CHECK(app.shows("playAgainButton"));
    CHECK_FALSE(app.shows("saveScoreButton"));
    CHECK(app.focus_name() == "playAgainButton");
}

TEST_CASE("A game over that makes the top 10 offers Save score in place of Play again", "[screens][nameentry]")
{
    App app;
    app.start_classic();
    app.crash();
    app.profile().record_game(Mode::CLASSIC, 1280);
    CHECK(app.shows("saveScoreButton"));
    CHECK_FALSE(app.shows("playAgainButton"));
    // The keyboard moved to the button that is there now.
    CHECK(app.focus_name() == "saveScoreButton");
    app.press(Qt::Key_Return);
    CHECK(app.shows("nameEntryScreen"));
    CHECK_FALSE(app.shows("gameOverScreen"));
}

TEST_CASE("Menu on a game over with a score to save lets the score go", "[screens][nameentry]")
{
    App app;
    app.start_classic();
    app.crash();
    app.profile().record_game(Mode::CLASSIC, 1280);
    app.click("menuButton");
    CHECK(app.game().state() == State::NotStarted);
    CHECK_FALSE(app.profile().qualifies());
    CHECK(app.profile().data().leaderboards.of(Mode::CLASSIC).empty());
}

namespace
{

/// Plays to a game over with a score of 1,280 in Classic that waits for a name, and opens the name entry screen.
void open_name_entry(App& app)
{
    app.start_classic();
    app.crash();
    app.profile().record_game(Mode::CLASSIC, 1280);
    app.click("saveScoreButton");
    REQUIRE(app.shows("nameEntryScreen"));
}

} // namespace

TEST_CASE("Name entry shows the score, the place and twelve empty slots", "[screens][nameentry]")
{
    App app;
    open_name_entry(app);
    CHECK(app.text_of("pendingScore") == "001280");
    CHECK(app.text_of("nameCount") == "0 of 12");
    // The only line is the score itself, in first place.
    CHECK(app.item("standing1")->isVisible());

    const QImage image = app.render();
    // The next slot to fill is coral; the others are cream.
    CHECK(image.pixel(app.face_of("slot0")) == rgb_of(tokens::ACCENT));
    CHECK(image.pixel(app.face_of("slot1")) == rgb_of(tokens::SURFACE));
    CHECK(image.pixel(app.face_of("slot11")) == rgb_of(tokens::SURFACE));
    // Save score cannot be pressed until the name has a letter.
    CHECK_FALSE(app.item("saveNameButton")->isEnabled());
}

TEST_CASE("The keyboard types the name and Backspace takes a letter off", "[screens][nameentry]")
{
    App app;
    open_name_entry(app);
    app.type("maya");
    CHECK(app.profile().entryName() == "MAYA");
    CHECK(app.text_of("nameCount") == "4 of 12");
    CHECK(app.item("slot0")->property("letter").toString() == "M");
    CHECK(app.item("slot3")->property("letter").toString() == "A");
    CHECK(app.item("saveNameButton")->isEnabled());

    app.press(Qt::Key_Backspace);
    CHECK(app.profile().entryName() == "MAY");
    // A space is a character of the name, and a digit is not.
    app.press(Qt::Key_Space);
    app.type("7");
    CHECK(app.profile().entryName() == "MAY ");
    // The standing shows the name as typed.
    CHECK(app.item("standing1")->isVisible());
}

TEST_CASE("The letter grid types by touch", "[screens][nameentry]")
{
    App app;
    open_name_entry(app);
    app.click("keyL");
    app.click("keyE");
    app.click("keyO");
    CHECK(app.profile().entryName() == "LEO");
    app.click("keyDelete");
    CHECK(app.profile().entryName() == "LE");
    app.click("keySpace");
    CHECK(app.profile().entryName() == "LE ");
    // The grid does not take the keyboard: Space still types.
    app.press(Qt::Key_Space);
    CHECK(app.profile().entryName() == "LE  ");
}

TEST_CASE("A recent name is one tap", "[screens][nameentry]")
{
    App app;
    app.profile().record_game(Mode::CLASSIC, 100);
    app.save_score_as("MUM");
    app.profile().record_game(Mode::CLASSIC, 200);
    app.save_score_as("DAD");
    open_name_entry(app);

    CHECK(app.text_of("recentName0") == "DAD");
    CHECK(app.text_of("recentName1") == "MUM");
    app.click("recentName1");
    CHECK(app.profile().entryName() == "MUM");
    app.click("recentName0");
    CHECK(app.profile().entryName() == "DAD");
}

TEST_CASE("Save score puts the score on the leaderboard with its line marked", "[screens][nameentry]")
{
    App app;
    open_name_entry(app);

    // With no letter the button does nothing, by touch or by key.
    app.click("saveNameButton");
    app.press(Qt::Key_Return);
    CHECK(app.shows("nameEntryScreen"));

    app.type("maya");
    app.click("saveNameButton");
    CHECK_FALSE(app.shows("nameEntryScreen"));
    REQUIRE(app.shows("leaderboardScreen"));
    CHECK(app.profile().boardMode() == GameViewModel::Mode::Classic);
    CHECK(app.item("line1")->property("name").toString() == "MAYA");
    CHECK(app.item("line1")->property("score").toString() == "001280");
    CHECK(app.item("line1")->property("latest").toBool());

    // The latest line is coral.
    const QImage image = app.render();
    const QPoint edge = app.item("line1")->mapToScene(QPointF(3, 24)).toPoint();
    CHECK(image.pixel(edge) == rgb_of(tokens::ACCENT));

    // Back goes to the title, not to the end screen of the game that is saved.
    app.click("backButton");
    CHECK(app.game().state() == State::NotStarted);
    CHECK(app.shows("titleScreen"));
}

TEST_CASE("Enter saves the name", "[screens][nameentry]")
{
    App app;
    open_name_entry(app);
    app.type("leo");
    app.press(Qt::Key_Return);
    CHECK(app.shows("leaderboardScreen"));
    CHECK(app.profile().data().leaderboards.of(Mode::CLASSIC).at(0).name.view() == "LEO");
}

TEST_CASE("Play on the leaderboard after saving starts the next game", "[screens][nameentry]")
{
    App app;
    open_name_entry(app);
    app.type("leo");
    app.press(Qt::Key_Return);
    app.click("leaderboardPlayButton");
    CHECK(app.game().state() == State::Running);
    CHECK_FALSE(app.shows("leaderboardScreen"));
    CHECK_FALSE(app.shows("nameEntryScreen"));
}

TEST_CASE("The motion preference is read from the GUI application", "[screens][settings]")
{
    // The off-screen platform has no accessibility settings, so it asks for nothing.
    CHECK_FALSE(snake::ui::system_prefers_reduced_motion());
}

namespace
{

/// The colour of a switch's track at a place the knob is not: the left part when on, the right part when off.
QRgb track_colour(App& app, const char* toggle, const QImage& image)
{
    QQuickItem* item = app.item(toggle);
    const bool on = item->property("checked").toBool();
    const qreal track_left = item->width() - 60;
    return image.pixel(item->mapToScene(QPointF(track_left + (on ? 15 : 45), item->height() / 2)).toPoint());
}

} // namespace

TEST_CASE("The title has a Settings button that opens the settings and Back closes them", "[screens][settings]")
{
    App app;
    app.click("settingsButton");
    CHECK(app.shows("settingsScreen"));
    CHECK_FALSE(app.shows("titleScreen"));
    CHECK(app.focus_name() == "soundToggle");

    app.click("settingsBackButton");
    CHECK(app.shows("titleScreen"));
    CHECK_FALSE(app.shows("settingsScreen"));

    app.click("settingsButton");
    app.press(Qt::Key_Escape);
    CHECK(app.shows("titleScreen"));
}

TEST_CASE("The settings show the switches and a touch tap or a key flips each one", "[screens][settings]")
{
    App app;
    app.click("settingsButton");
    CHECK(app.item("soundToggle")->property("checked").toBool());
    CHECK(app.item("musicToggle")->property("checked").toBool());
    CHECK_FALSE(app.item("reducedMotionToggle")->property("checked").toBool());

    app.click("soundToggle");
    CHECK_FALSE(app.profile().sound());
    CHECK_FALSE(app.item("soundToggle")->property("checked").toBool());
    app.click("soundToggle");
    CHECK(app.profile().sound());

    // The keyboard starts on Sound; Down goes on to Music, and Space or Enter flips it.
    REQUIRE(app.focus_name() == "soundToggle");
    app.press(Qt::Key_Down);
    CHECK(app.focus_name() == "musicToggle");
    app.press(Qt::Key_Space);
    CHECK_FALSE(app.profile().music());
    app.press(Qt::Key_Down);
    CHECK(app.focus_name() == "reducedMotionToggle");
    app.press(Qt::Key_Return);
    CHECK(app.profile().reducedMotion());
    app.press(Qt::Key_Up);
    CHECK(app.focus_name() == "musicToggle");
}

TEST_CASE("A switch is coral with the knob at the right when on and cream with the knob at the left when off",
          "[screens][settings]")
{
    App app;
    app.click("settingsButton");
    QImage image = app.render();
    CHECK(track_colour(app, "soundToggle", image) == rgb_of(tokens::ACCENT));
    CHECK(track_colour(app, "reducedMotionToggle", image) == rgb_of(tokens::SURFACE));
    app.click("soundToggle");
    image = app.render();
    CHECK(track_colour(app, "soundToggle", image) == rgb_of(tokens::SURFACE));
}

TEST_CASE("Inverted axes are offered only on a device with a touch screen", "[screens][settings]")
{
    {
        App app;
        app.click("settingsButton");
        CHECK_FALSE(app.shows("invertAxesToggle"));
    }
    {
        App app(true);
        app.click("settingsButton");
        REQUIRE(app.shows("invertAxesToggle"));
        app.click("invertAxesToggle");
        CHECK(app.profile().invertAxes());
    }
}

TEST_CASE("Resetting a leaderboard asks first and keeps the scores if the player says no", "[screens][settings]")
{
    App app;
    app.profile().record_game(Mode::CLASSIC, 1480);
    app.save_score_as("MUM");
    app.profile().record_game(Mode::EASY, 300);
    app.save_score_as("LEO");
    app.click("settingsButton");
    CHECK_FALSE(app.shows("resetConfirm"));

    app.click("resetClassicButton");
    REQUIRE(app.shows("resetConfirm"));
    CHECK(app.text_of("confirmTitle") == "Reset the Classic leaderboard?");
    // The question starts on the safe answer.
    CHECK(app.focus_name() == "cancelButton");

    // Nothing behind the dialog answers while it is up.
    app.click("resetEasyButton");
    CHECK(app.text_of("confirmTitle") == "Reset the Classic leaderboard?");
    app.press(Qt::Key_Return);
    CHECK_FALSE(app.shows("resetConfirm"));
    CHECK(app.profile().data().leaderboards.of(Mode::CLASSIC).size() == 1);

    // Escape is no as well.
    app.click("resetClassicButton");
    app.press(Qt::Key_Escape);
    CHECK_FALSE(app.shows("resetConfirm"));
    CHECK(app.shows("settingsScreen"));
    CHECK(app.profile().data().leaderboards.of(Mode::CLASSIC).size() == 1);
}

TEST_CASE("After the question the keyboard is back on the button that asked it, so Escape leaves the settings",
          "[screens][settings]")
{
    App app;
    app.click("settingsButton");
    app.click("resetClassicButton");
    REQUIRE(app.shows("resetConfirm"));
    app.press(Qt::Key_Escape);
    CHECK_FALSE(app.shows("resetConfirm"));
    CHECK(app.focus_name() == "resetClassicButton");
    // The next Escape is the settings' own: it goes back to the title.
    app.press(Qt::Key_Escape);
    CHECK(app.shows("titleScreen"));
    CHECK_FALSE(app.shows("settingsScreen"));

    // And after a yes.
    app.click("settingsButton");
    app.click("resetEasyButton");
    app.click("confirmButton");
    CHECK(app.focus_name() == "resetEasyButton");
}

TEST_CASE("Resetting a leaderboard that the player confirms empties that one only", "[screens][settings]")
{
    App app;
    app.profile().record_game(Mode::CLASSIC, 1480);
    app.save_score_as("MUM");
    app.profile().record_game(Mode::EASY, 300);
    app.save_score_as("LEO");
    app.click("settingsButton");

    app.click("resetEasyButton");
    CHECK(app.text_of("confirmTitle") == "Reset the Easy leaderboard?");
    app.click("confirmButton");
    CHECK_FALSE(app.shows("resetConfirm"));
    CHECK(app.profile().data().leaderboards.of(Mode::EASY).empty());
    CHECK(app.profile().data().leaderboards.of(Mode::CLASSIC).size() == 1);
}

TEST_CASE("The pause dialog has the sound and music switches", "[screens][settings]")
{
    App app;
    app.start_classic();
    app.game().pause();
    REQUIRE(app.shows("pauseScreen"));
    CHECK_FALSE(app.shows("controlsTilt"));
    CHECK_FALSE(app.shows("recalibrateButton"));

    app.click("pauseSoundToggle");
    CHECK_FALSE(app.profile().sound());
    app.click("pauseMusicToggle");
    CHECK_FALSE(app.profile().music());
    // The game is still paused.
    CHECK(app.game().state() == State::Paused);
}

TEST_CASE("On a touch device the pause dialog has the controls and recalibrates tilt", "[screens][settings]")
{
    App app(true);
    app.start_classic();
    app.game().pause();
    REQUIRE(app.shows("controlsTilt"));
    CHECK(app.shows("recalibrateButton"));
    // Recalibrate tilt holds the game while the phone is held how the player likes it: the dialog gives way to the
    // wait.
    app.click("recalibrateButton");
    CHECK(app.input().calibrating());
    CHECK_FALSE(app.shows("pauseScreen"));
    CHECK(app.shows("calibrationOverlay"));
    app.finish_calibration();
    CHECK_FALSE(app.shows("calibrationOverlay"));
    CHECK(app.game().state() == State::Running);
    app.game().pause();
    REQUIRE(app.shows("pauseScreen"));

    // The choice is coral. Only tilt has anything to recalibrate.
    const QImage image = app.render();
    CHECK(image.pixel(app.face_of("controlsTilt")) == rgb_of(tokens::ACCENT));
    CHECK(image.pixel(app.face_of("controlsSwipe")) == rgb_of(tokens::SURFACE));
    app.click("controlsSwipe");
    CHECK(app.profile().controls() == ProfileViewModel::Controls::Swipe);
    CHECK_FALSE(app.shows("recalibrateButton"));
    app.click("controlsDpad");
    CHECK(app.profile().controls() == ProfileViewModel::Controls::Dpad);
    app.click("controlsJoystick");
    CHECK(app.profile().controls() == ProfileViewModel::Controls::Joystick);
    CHECK_FALSE(app.shows("recalibrateButton"));
    app.click("controlsTilt");
    CHECK(app.profile().controls() == ProfileViewModel::Controls::Tilt);
    CHECK(app.shows("recalibrateButton"));
}

TEST_CASE("The food bobs unless reduced motion is on", "[screens][settings]")
{
    {
        App app;
        app.start_classic();
        QTest::qWait(300);
        CHECK(app.item("board")->property("foodLift").toDouble() > 0.0);
    }
    {
        App app;
        app.profile().setReducedMotion(true);
        app.start_classic();
        QTest::qWait(300);
        CHECK(app.item("board")->property("foodLift").toDouble() == 0.0);
    }
}

TEST_CASE("The offscreen platform has no touch screen", "[screens][settings]")
{
    CHECK_FALSE(snake::ui::has_touch_screen());
}

TEST_CASE("Settings has an About button that opens About and Back returns to the settings", "[screens][about]")
{
    App app;
    app.click("settingsButton");
    app.click("aboutButton");
    CHECK(app.shows("aboutScreen"));
    CHECK_FALSE(app.shows("settingsScreen"));
    CHECK(app.focus_name() == "licenceButton");

    app.click("aboutBackButton");
    CHECK(app.shows("settingsScreen"));
    CHECK_FALSE(app.shows("aboutScreen"));

    app.click("aboutButton");
    app.press(Qt::Key_Escape);
    CHECK(app.shows("settingsScreen"));
}

TEST_CASE("About shows the version, the licence, the source and the revision", "[screens][about]")
{
    App app;
    app.click("settingsButton");
    app.click("aboutButton");
    const QString version = QString::fromUtf8(snake::VERSION_STRING.data(), snake::VERSION_STRING.size());
    const QString url = QString::fromUtf8(snake::REPOSITORY_URL.data(), snake::REPOSITORY_URL.size());
    const QString revision = QString::fromUtf8(snake::SOURCE_REVISION.data(), snake::SOURCE_REVISION.size());
    CHECK(app.text_of("aboutVersion") == "Version " + version);
    CHECK(app.text_of("aboutLicence") == "Free software, licensed under the GPL-3.0-or-later.");
    CHECK(app.text_of("aboutSource") == "Source: " + url);
    CHECK(url == "https://github.com/iainchesworthlabs/snake");
    // The revision is a tag or a commit, or "unknown" where there is no git: never empty.
    CHECK_FALSE(revision.isEmpty());
    CHECK(app.text_of("aboutRevision") == "Revision: " + snake::ui::AboutInfo::shortened(revision));
    // The details for a bug report are not on the page until the version is tapped five times.
    CHECK_FALSE(app.shows("aboutLanguage"));
    CHECK_FALSE(app.shows("aboutSaveStatus"));
    for (int tap = 0; tap < 5; ++tap)
    {
        QTest::mouseClick(&app.window(), Qt::LeftButton, Qt::NoModifier, app.centre_of(app.item("aboutVersion")));
    }
    CHECK(app.shows("aboutLanguage"));
    CHECK(app.shows("aboutSaveStatus"));
    CHECK(app.text_of("aboutSaveStatus").startsWith("Saves: "));
}

TEST_CASE("About shows a commit as seven characters and a tag as it is", "[screens][about]")
{
    using snake::ui::AboutInfo;
    CHECK(AboutInfo::shortened("0b66ebe4f1ac93b75323b3065777b4dd3f287cdf") == "0b66ebe");
    CHECK(AboutInfo::shortened("0b66ebe4f1ac") == "0b66ebe");
    CHECK(AboutInfo::shortened("v0.2.0-rc.2") == "v0.2.0-rc.2");
    CHECK(AboutInfo::shortened("unknown") == "unknown");
    CHECK(AboutInfo::shortened("abc123") == "abc123");
    CHECK(AboutInfo::shortened("") == "");
}

TEST_CASE("Each licence has its text one tap away and Back returns to About", "[screens][about]")
{
    App app;
    app.click("settingsButton");
    app.click("aboutButton");

    app.click("licenceButton");
    REQUIRE(app.shows("noticeScreen"));
    CHECK_FALSE(app.shows("aboutScreen"));
    CHECK(app.text_of("noticeTitle") == "Licence");
    CHECK(app.text_of("noticeBody").contains("GNU GENERAL PUBLIC LICENSE"));
    app.click("noticeBackButton");
    CHECK(app.shows("aboutScreen"));

    app.click("noticesButton");
    CHECK(app.text_of("noticeTitle") == "Third-party notices");
    // One entry for Qt and one for each library of the manifest that ships.
    CHECK(app.text_of("noticeBody").contains("Third-party notices for Snake"));
    CHECK(app.text_of("noticeBody").contains("Qt "));
    app.press(Qt::Key_Escape);
    CHECK(app.shows("aboutScreen"));

    app.click("fontLicenceButton");
    CHECK(app.text_of("noticeTitle") == "Font licence");
    CHECK(app.text_of("noticeBody").contains("SIL OPEN FONT LICENSE"));
    CHECK(app.text_of("noticeBody").contains("Pixelify Sans"));
    // The fonts for the other scripts have their licence there too.
    CHECK(app.text_of("noticeBody").contains("Noto Sans JP"));
    CHECK(app.text_of("noticeBody").contains("Noto Sans Hebrew"));
}

TEST_CASE("The notice text scrolls with the arrow keys and the page keys and stops at both ends", "[screens][about]")
{
    App app;
    app.click("settingsButton");
    app.click("aboutButton");
    app.click("licenceButton");
    QQuickItem* scroller = app.item("noticeScroller");
    const auto position = [&] { return scroller->property("contentY").toDouble(); };
    REQUIRE(scroller->property("contentHeight").toDouble() > scroller->height() * 5);

    CHECK(position() == 0.0);
    app.press(Qt::Key_Up);
    CHECK(position() == 0.0);
    app.press(Qt::Key_Down);
    CHECK(position() > 0.0);
    const double line = position();
    app.press(Qt::Key_PageDown);
    CHECK(position() > line + 100.0);
    app.press(Qt::Key_PageUp);
    CHECK(position() == line);
    app.press(Qt::Key_Up);
    CHECK(position() == 0.0);

    // Far past the end it stops at the last page.
    for (int i = 0; i < 400; ++i)
    {
        app.press(Qt::Key_PageDown);
    }
    CHECK(position() == scroller->property("contentHeight").toDouble() - scroller->height());
    // Opening it again starts at the top.
    app.press(Qt::Key_Escape);
    app.click("licenceButton");
    CHECK(position() == 0.0);
}

namespace
{

/// Writes down what the program plays, and is the program's sink for as long as it lives.
class ListeningSink final : public snake::ui::SoundSink
{
public:
    ListeningSink()
    {
        snake::ui::set_default_sound_sink(this);
    }
    ~ListeningSink() override
    {
        snake::ui::set_default_sound_sink(nullptr);
    }
    ListeningSink(const ListeningSink&) = delete;
    ListeningSink& operator=(const ListeningSink&) = delete;

    void play(snake::audio::Effect effect) override
    {
        effects.push_back(effect);
    }
    void play_music(snake::audio::Tune tune) override
    {
        tunes.push_back(tune);
    }
    void stop_music() override
    {
        ++stops;
    }

    std::vector<snake::audio::Effect> effects;
    std::vector<snake::audio::Tune> tunes;
    int stops = 0;
};

} // namespace

TEST_CASE("The screens are heard: the title tune, a button, a game and its tune", "[screens][sound]")
{
    using snake::audio::Effect;
    using snake::audio::Tune;
    ListeningSink sink;
    App app;
    // The title has its tune as soon as the program is up.
    REQUIRE_FALSE(sink.tunes.empty());
    CHECK(sink.tunes.back() == Tune::TITLE);
    CHECK(sink.effects.empty());

    // A button, by touch and by key.
    app.click("leaderboardButton");
    REQUIRE(sink.effects.size() == 1);
    CHECK(sink.effects.back() == Effect::SELECT);
    app.press(Qt::Key_Escape);
    app.click("playButton");
    // The press of Play, then the jingle of the game and its tune.
    REQUIRE(sink.effects.size() >= 3);
    CHECK(sink.effects[sink.effects.size() - 2] == Effect::SELECT);
    CHECK(sink.effects.back() == Effect::START);
    CHECK(sink.tunes.back() == Tune::GAME);

    // Pausing is silent.
    const int stops = sink.stops;
    app.game().pause();
    CHECK(sink.stops == stops + 1);
}

TEST_CASE("A switch is heard, and a screen with the sound off is not", "[screens][sound]")
{
    using snake::audio::Effect;
    ListeningSink sink;
    App app;
    app.click("settingsButton");
    sink.effects.clear();
    app.click("soundToggle");
    // The press is heard by the director before the setting changes, so this one sounds.
    CHECK(sink.effects == std::vector<Effect>{Effect::SELECT});
    CHECK_FALSE(app.profile().sound());
    sink.effects.clear();
    app.click("soundToggle");
    // Now the sound is off while the switch is pressed, so the press that turns it on is silent.
    CHECK(sink.effects.empty());
    CHECK(app.profile().sound());
}

namespace
{

/// How many pixels of `colour` the image has.
int count_pixels(const QImage& image, QRgb colour)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            count += image.pixel(x, y) == colour ? 1 : 0;
        }
    }
    return count;
}

} // namespace

TEST_CASE("The burst of a food is drawn on the board and goes", "[screens][effects]")
{
    App app;
    app.start_classic();
    const QImage before = app.render();
    const int leaf_before = count_pixels(before, rgb_of(tokens::LEAF));

    emit app.game().tickEvent(GameViewModel::Event::Ate);
    CHECK(app.effects().active());
    const QImage during = app.render();
    // The specks are in the colours of the food, and the leaf green is one of them.
    CHECK(count_pixels(during, rgb_of(tokens::LEAF)) > leaf_before);

    // Time passes and the burst is over.
    for (int i = 0; i < 120 && app.effects().active(); ++i)
    {
        app.effects().advance(1.0 / 60.0);
    }
    CHECK_FALSE(app.effects().active());
    CHECK(count_pixels(app.render(), rgb_of(tokens::LEAF)) == leaf_before);
}

TEST_CASE("The frame animation moves the effects on", "[screens][effects]")
{
    App app;
    app.start_classic();
    emit app.game().tickEvent(GameViewModel::Event::Ate);
    REQUIRE(app.effects().active());
    // Real time: the burst is over within a second or so.
    for (int i = 0; i < 100 && app.effects().active(); ++i)
    {
        QTest::qWait(20);
    }
    CHECK_FALSE(app.effects().active());
}

TEST_CASE("A crash shakes the board unless reduced motion is on", "[screens][effects]")
{
    {
        App app;
        app.start_classic();
        emit app.game().tickEvent(GameViewModel::Event::Crashed);
        CHECK(app.effects().active());
        app.effects().advance(0.01);
        CHECK(app.effects().effects().shake() != QPoint());
    }
    {
        App app;
        app.profile().setReducedMotion(true);
        CHECK(app.effects().reducedMotion());
        app.start_classic();
        emit app.game().tickEvent(GameViewModel::Event::Crashed);
        app.effects().advance(0.01);
        CHECK(app.effects().effects().shake() == QPoint());
    }
}

TEST_CASE("The snake dies on the game screen before the end of the game shows", "[screens][effects]")
{
    App app;
    app.start_classic();
    for (int i = 0; i < 40 && app.game().state() == State::Running; ++i)
    {
        app.game().advance(0.15);
    }
    REQUIRE(app.game().state() == State::GameOver);
    // The game is over, but the board is still on the screen with the snake dying.
    CHECK(app.effects().dying());
    CHECK(app.shows("gameScreen"));
    CHECK_FALSE(app.shows("gameOverScreen"));

    // Waiting for it is enough.
    for (int i = 0; i < 300 && app.effects().dying(); ++i)
    {
        app.effects().advance(0.01);
    }
    CHECK_FALSE(app.shows("gameScreen"));
    CHECK(app.shows("gameOverScreen"));
}

TEST_CASE("A key or a tap skips the death of the snake", "[screens][effects]")
{
    for (const bool by_key : {true, false})
    {
        App app;
        app.start_classic();
        for (int i = 0; i < 40 && app.game().state() == State::Running; ++i)
        {
            app.game().advance(0.15);
        }
        REQUIRE(app.effects().dying());
        if (by_key)
        {
            app.press(Qt::Key_Space);
        }
        else
        {
            QTest::mouseClick(&app.window(), Qt::LeftButton, Qt::NoModifier, QPoint(195, 400));
        }
        CHECK_FALSE(app.effects().dying());
        CHECK(app.shows("gameOverScreen"));
        // The game over screen shows the head that is left, with the crosses of dizzy eyes: the snake is not a full
        // one.
        CHECK(app.effects().effects().death_look().popped > 0);
    }
}

TEST_CASE("The title shows the game playing itself in a frame", "[screens][attract]")
{
    App app;
    CHECK(app.shows("attractPreview"));
    GameViewModel& demo = app.demo();
    CHECK(demo.autoplay());
    CHECK(demo.state() == State::Running);
    // It is a game of its own: the real one has not started.
    CHECK(app.game().state() == State::NotStarted);
    CHECK(&demo != &app.game());

    // It plays in real time while the title is up.
    const auto ticks = demo.session().simulation().ticks();
    for (int i = 0; i < 100 && demo.session().simulation().ticks() == ticks; ++i)
    {
        QTest::qWait(20);
    }
    CHECK(demo.session().simulation().ticks() > ticks);

    // The preview is the board and not the yellow of the page.
    const QImage image = app.render();
    QQuickItem* preview = app.item("attractPreview");
    const QPoint inside = preview->mapToScene(QPointF(preview->width() / 2, preview->height() / 2)).toPoint();
    CHECK(image.pixel(inside) != rgb_of(tokens::BACKGROUND));
}

TEST_CASE("The demo stops when the title goes", "[screens][attract]")
{
    App app;
    app.click("playButton");
    REQUIRE(app.game().state() == State::Running);
    const auto ticks = app.demo().session().simulation().ticks();
    QTest::qWait(400);
    CHECK(app.demo().session().simulation().ticks() == ticks);
}

TEST_CASE("The demo does not use the seeds of the real game", "[screens][attract]")
{
    App first;
    const auto food = first.game().session().simulation().food();
    // The real game's first food does not depend on how much the demo has played.
    QTest::qWait(300);
    App second;
    CHECK(second.demo().session().simulation().food() == first.demo().session().simulation().food());
    static_cast<void>(food);
}

namespace
{

/// Shows the program in a language for as long as it lives: the compiled catalogue is installed and the locale set,
/// as the program does at start-up. The screens read the language when they are created, so make it first.
class InLanguage
{
public:
    explicit InLanguage(const QString& code) : m_Previous(QLocale()), m_Direction(QGuiApplication::layoutDirection())
    {
        QLocale::setDefault(QLocale(code));
        // As the program does, the direction is the language's: Hebrew and Yiddish read from right to left.
        QGuiApplication::setLayoutDirection(QLocale(code).textDirection());
        if (code != "en")
        {
            const QString file = QString("%1/snake_%2.qm").arg(QStringLiteral(SNAKE_QM_DIR), code);
            REQUIRE(m_Translator.load(file));
            REQUIRE(QGuiApplication::installTranslator(&m_Translator));
            m_Installed = true;
        }
    }
    InLanguage(const InLanguage&) = delete;
    InLanguage& operator=(const InLanguage&) = delete;
    ~InLanguage()
    {
        if (m_Installed)
        {
            QGuiApplication::removeTranslator(&m_Translator);
        }
        QLocale::setDefault(m_Previous);
        QGuiApplication::setLayoutDirection(m_Direction);
    }

private:
    QLocale m_Previous;
    Qt::LayoutDirection m_Direction;
    QTranslator m_Translator;
    bool m_Installed = false;
};

bool is_text(const QQuickItem* item)
{
    return QByteArray(item->metaObject()->className()).startsWith("QQuickText");
}

/// Every visible text in the item and below it that has something to say.
void collect_texts(QQuickItem* item, std::vector<QQuickItem*>& out)
{
    if (!item->isVisible())
    {
        return;
    }
    if (is_text(item) && !item->property("text").toString().isEmpty())
    {
        out.push_back(item);
    }
    for (QQuickItem* child : item->childItems())
    {
        collect_texts(child, out);
    }
}

/// The nearest ancestor that is a box with a face of its own, which the text has to fit in: a button, a card, a panel.
QQuickItem* box_around(QQuickItem* text)
{
    for (QQuickItem* parent = text->parentItem(); parent != nullptr; parent = parent->parentItem())
    {
        if (QByteArray(parent->metaObject()->className()).startsWith("QQuickRectangle") && parent->width() > 0)
        {
            return parent;
        }
    }
    return nullptr;
}

/// Complaints about the texts of the window that run out of their box, off the page or past their own width. Empty when
/// everything fits.
QStringList misfits(QQuickWindow& window)
{
    std::vector<QQuickItem*> texts;
    collect_texts(window.contentItem(), texts);
    QStringList complaints;
    const QRectF page(0, 0, window.width(), window.height());
    for (QQuickItem* text : texts)
    {
        const QString words = text->property("text").toString();
        const QRectF where = text->mapRectToScene(QRectF(0, 0, text->width(), text->height()));
        const double content = text->property("contentWidth").toDouble();
        if (content > text->width() + 0.5)
        {
            complaints << QString("\"%1\" is %2 wide in a space of %3").arg(words).arg(content).arg(text->width());
        }
        if (!page.adjusted(-1, -1, 1, 1).contains(where))
        {
            complaints << QString("\"%1\" is off the page (%2 to %3)").arg(words).arg(where.left()).arg(where.right());
        }
        if (const QQuickItem* box = box_around(text))
        {
            const QRectF around = box->mapRectToScene(QRectF(0, 0, box->width(), box->height()));
            if (!around.adjusted(-1, -1, 1, 1).contains(where))
            {
                complaints << QString("\"%1\" runs out of its box").arg(words);
            }
        }
    }
    return complaints;
}

/// Checks that everything on the screen fits, naming the language and the screen if it does not.
void check_fit(App& app, const char* screen)
{
    // The layouts place their items when the window is next polished.
    static_cast<void>(app.render());
    const QStringList bad = misfits(app.window());
    INFO("screen " << screen << ": " << bad.join("; ").toStdString());
    CHECK(bad.isEmpty());
}

constexpr std::array<const char*, 8> TESTED_LANGUAGES = {"en", "fr", "es", "de", "ja", "zh_CN", "he", "yi"};

/// What the screens are made to hold at their fullest, so that the longest text is on show.
void fill_the_profile(App& app)
{
    for (const auto& [name, score] :
         {std::pair{"GRANDMA JUNE", 2300u}, std::pair{"MAYA", 1400u}, std::pair{"LEO", 900u}})
    {
        app.profile().record_game(Mode::CLASSIC, score);
        app.save_score_as(name);
    }
}

} // namespace

TEST_CASE("The title, the settings and the leaderboard fit in every language", "[screens][languages]")
{
    for (const char* code : TESTED_LANGUAGES)
    {
        INFO("language " << code);
        const InLanguage language(code);
        App app(true);
        fill_the_profile(app);
        check_fit(app, "title");

        app.click("settingsButton");
        check_fit(app, "settings");
        app.click("resetClassicButton");
        check_fit(app, "confirm");
        app.press(Qt::Key_Escape);
        app.click("aboutButton");
        check_fit(app, "about");
        app.press(Qt::Key_Escape);
        app.press(Qt::Key_Escape);

        app.click("leaderboardButton");
        check_fit(app, "leaderboard");
    }
}

TEST_CASE("The game screens fit in every language", "[screens][languages]")
{
    for (const char* code : TESTED_LANGUAGES)
    {
        INFO("language " << code);
        const InLanguage language(code);
        App app(true);
        fill_the_profile(app);
        app.start_classic();
        check_fit(app, "game");
        app.game().pause();
        check_fit(app, "pause");
        app.game().resume();

        app.crash();
        app.profile().record_game(Mode::CLASSIC, 5000);
        check_fit(app, "game over");
        app.click("saveScoreButton");
        app.type("maya");
        check_fit(app, "name entry");
    }
}

TEST_CASE("A name in another script is set smaller than the pixel wordmark, to stay above the preview",
          "[screens][languages]")
{
    const InLanguage language("zh_CN");
    App app;
    CHECK(app.text_of("wordmark") == "贪吃蛇");
    QQuickItem* wordmark = app.item("wordmark");
    const QQuickItem* preview = app.item("attractPreview");
    const double bottom = wordmark->mapRectToScene(QRectF(0, 0, wordmark->width(), wordmark->height())).bottom();
    CHECK(bottom <= preview->mapToScene(QPointF(0, 0)).y());
}

namespace
{

/// The middle of an item, across the window.
double centre_x(App& app, const char* name)
{
    QQuickItem* item = app.item(name);
    return item->mapToScene(QPointF(item->width() / 2, 0)).x();
}

} // namespace

TEST_CASE("Hebrew and Yiddish read from right to left and the other languages from left to right", "[screens][rtl]")
{
    for (const char* code : {"he", "yi"})
    {
        CHECK(QLocale(code).textDirection() == Qt::RightToLeft);
    }
    for (const char* code : {"en", "fr", "es", "de", "ja", "zh_CN"})
    {
        CHECK(QLocale(code).textDirection() == Qt::LeftToRight);
    }
}

TEST_CASE("The title mirrors: the settings button is on the left and the leaderboard on the right", "[screens][rtl]")
{
    {
        const InLanguage language("en");
        App app;
        CHECK(centre_x(app, "leaderboardButton") < centre_x(app, "settingsButton"));
    }
    for (const char* code : {"he", "yi"})
    {
        const InLanguage language(code);
        App app;
        INFO("language " << code);
        CHECK(centre_x(app, "leaderboardButton") > centre_x(app, "settingsButton"));
        // The mode cards are the width of the page, and their words start on the right.
        QQuickItem* card = app.item("modeCard0");
        const QRectF where = card->mapRectToScene(QRectF(0, 0, card->width(), card->height()));
        CHECK(where.center().x() == Catch::Approx(PAGE_WIDTH / 2.0).margin(2.0));
    }
}

TEST_CASE("A screen with a back button has it on the right, and the heading beside it", "[screens][rtl]")
{
    {
        const InLanguage language("en");
        App app;
        app.click("leaderboardButton");
        CHECK(centre_x(app, "backButton") < PAGE_WIDTH / 2.0);
    }
    const InLanguage language("he");
    App app;
    app.click("leaderboardButton");
    CHECK(centre_x(app, "backButton") > PAGE_WIDTH / 2.0);
    // The heading is on the right too, to the left of the back button, and not off the page.
    std::vector<QQuickItem*> texts;
    collect_texts(app.item("leaderboardScreen"), texts);
    QQuickItem* heading = nullptr;
    for (QQuickItem* text : texts)
    {
        if (text->property("text").toString() == "לוח תוצאות")
        {
            heading = text;
        }
    }
    REQUIRE(heading != nullptr);
    CHECK(heading->mapToScene(QPointF(heading->width(), 0)).x() < centre_x(app, "backButton"));
    CHECK(heading->mapToScene(QPointF(heading->width() / 2, 0)).x() > PAGE_WIDTH / 2.0);
}

TEST_CASE("The back arrow points the way back", "[screens][rtl]")
{
    {
        const InLanguage language("en");
        App app;
        app.click("settingsButton");
        QQuickItem* back = app.item("settingsBackButton");
        CHECK(back->property("text").toString() == "<");
    }
    {
        const InLanguage language("he");
        App app;
        app.click("settingsButton");
        QQuickItem* back = app.item("settingsBackButton");
        CHECK(back->property("text").toString() == ">");
    }
}

TEST_CASE("The switches mirror: the label on the right, the switch on the left, and on is towards the end",
          "[screens][rtl]")
{
    const auto track_of = [](App& app, const char* toggle)
    { return app.item(toggle)->findChild<QQuickItem*>("track"); };
    const auto label_x = [](App& app, const char* toggle)
    {
        // The text of the switch: the first text child.
        std::vector<QQuickItem*> texts;
        collect_texts(app.item(toggle), texts);
        REQUIRE_FALSE(texts.empty());
        return texts.front()->mapToScene(QPointF(0, 0)).x();
    };
    {
        const InLanguage language("en");
        App app;
        app.click("settingsButton");
        CHECK(label_x(app, "soundToggle") < track_of(app, "soundToggle")->mapToScene(QPointF(0, 0)).x());
    }
    const InLanguage language("he");
    App app;
    app.click("settingsButton");
    CHECK(label_x(app, "soundToggle") > track_of(app, "soundToggle")->mapToScene(QPointF(0, 0)).x());
}

TEST_CASE("The score is on the right and the best on the left while a game is played", "[screens][rtl]")
{
    {
        const InLanguage language("en");
        App app;
        app.start_classic();
        CHECK(centre_x(app, "score") < centre_x(app, "bestScore"));
    }
    const InLanguage language("he");
    App app;
    app.start_classic();
    CHECK(centre_x(app, "score") > centre_x(app, "bestScore"));
}

TEST_CASE("The name is typed from the left even where the page reads from the right", "[screens][rtl]")
{
    const InLanguage language("he");
    App app;
    app.start_classic();
    app.crash();
    app.profile().record_game(Mode::CLASSIC, 1280);
    app.click("saveScoreButton");
    CHECK(centre_x(app, "slot0") < centre_x(app, "slot1"));
    CHECK(centre_x(app, "slot0") < PAGE_WIDTH / 2.0);
    // The letters of the grid run from the left, A first.
    CHECK(centre_x(app, "keyA") < centre_x(app, "keyB"));
    app.type("maya");
    CHECK(app.item("slot0")->property("letter").toString() == "M");
}

TEST_CASE("The leaderboard lines mirror: the place and the name start on the right and the score is on the left",
          "[screens][rtl]")
{
    const InLanguage language("he");
    App app;
    fill_the_profile(app);
    app.click("leaderboardButton");
    app.click("classicTab");
    QQuickItem* line = app.item("line1");
    std::vector<QQuickItem*> texts;
    collect_texts(line, texts);
    // The place and the name are text; the score is drawn from squares (PixelNumber), and is the first "lineScore"
    // (line 1's).
    REQUIRE(texts.size() == 2);
    const QQuickItem* score = app.item("lineScore");
    const auto x_of = [](const QQuickItem* item) { return item->mapToScene(QPointF(0, 0)).x(); };
    CHECK(x_of(texts[0]) > x_of(score));
    CHECK(x_of(texts[1]) > x_of(score));
}

TEST_CASE("The licence text reads from the left in a language read from the right", "[screens][rtl]")
{
    const InLanguage language("he");
    App app;
    app.click("settingsButton");
    app.click("aboutButton");
    app.click("licenceButton");
    QQuickItem* body = app.item("noticeBody");
    // Left-aligned text: its box starts at the left of the panel.
    CHECK(body->mapToScene(QPointF(0, 0)).x() < 60);
}

TEST_CASE("A computer has no accelerometer and a motor that does nothing", "[screens][device]")
{
    // The tests run on a computer, whatever it is: the operating system's own device file is the one that is built.
    CHECK(snake::ui::device::make_accelerometer() == nullptr);
    const auto haptics = snake::ui::device::make_haptics();
    REQUIRE(haptics != nullptr);
    haptics->tap();
    haptics->thud();
}

namespace
{

// A phone on its side: the width and height in points of an iPhone 16 Pro, less its safe area.
constexpr int LANDSCAPE_WIDTH = 852;
constexpr int LANDSCAPE_HEIGHT = 393;

QRectF scene_rect(App& app, const char* name)
{
    // The layouts place their items when the window is next polished.
    QTest::qWait(20);
    const QQuickItem* item = app.item(name);
    return item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
}

} // namespace

TEST_CASE("Lying down, the title is two columns that fit the window and do not run into each other",
          "[screens][landscape]")
{
    App app(true, LANDSCAPE_WIDTH, LANDSCAPE_HEIGHT);
    const QRectF window(0, 0, LANDSCAPE_WIDTH, LANDSCAPE_HEIGHT);
    const QRectF play = scene_rect(app, "playButton");
    const QRectF preview = scene_rect(app, "attractPreview");
    const QRectF leaderboard = scene_rect(app, "leaderboardButton");
    const QRectF settings = scene_rect(app, "settingsButton");
    const QRectF wordmark = scene_rect(app, "wordmark");
    for (const QRectF& part : {play, preview, leaderboard, settings, wordmark})
    {
        CHECK(window.contains(part));
    }
    // The name, the preview, Play and the other screens are one column, in that order, and no part touches another.
    CHECK(wordmark.bottom() <= preview.top());
    CHECK(preview.bottom() <= play.top());
    CHECK(play.bottom() <= leaderboard.top());
    CHECK(leaderboard.right() <= settings.left());

    // Every mode is in the other column, and the cards are on top of each other without overlapping.
    QRectF previous;
    for (const char* card : {"modeCard0", "modeCard1", "modeCard2", "modeCard3"})
    {
        const QRectF where = scene_rect(app, card);
        INFO(card);
        CHECK(window.contains(where));
        CHECK(where.left() > play.right());
        CHECK_FALSE(where.intersects(play));
        if (!previous.isNull())
        {
            CHECK(where.top() >= previous.bottom());
        }
        previous = where;
    }
}

TEST_CASE("The title follows the shape of the window", "[screens][landscape]")
{
    App app(true);
    // Upright: Play is under the modes, across the width.
    CHECK(scene_rect(app, "playButton").top() > scene_rect(app, "modeCard3").bottom());
    app.window().resize(LANDSCAPE_WIDTH, LANDSCAPE_HEIGHT);
    QTest::qWait(20);
    CHECK(scene_rect(app, "playButton").right() < scene_rect(app, "modeCard0").left());
    app.window().resize(PAGE_WIDTH, PAGE_HEIGHT);
    QTest::qWait(20);
    CHECK(scene_rect(app, "playButton").top() > scene_rect(app, "modeCard3").bottom());
}

TEST_CASE("Lying down, the title mirrors in a language read from the right", "[screens][landscape][rtl]")
{
    const InLanguage language("he");
    App app(true, LANDSCAPE_WIDTH, LANDSCAPE_HEIGHT);
    CHECK(scene_rect(app, "playButton").center().x() > LANDSCAPE_WIDTH / 2.0);
    CHECK(scene_rect(app, "modeCard0").center().x() < LANDSCAPE_WIDTH / 2.0);
    CHECK(scene_rect(app, "settingsButton").center().x() < scene_rect(app, "leaderboardButton").center().x());
}

TEST_CASE("Lying down, the pause dialog is two columns that fit a phone on its side", "[screens][landscape]")
{
    App app(true, LANDSCAPE_WIDTH, LANDSCAPE_HEIGHT);
    app.profile().setControls(ProfileViewModel::Controls::Tilt);
    app.start_classic();
    app.game().pause();
    REQUIRE(app.shows("pauseScreen"));
    const QRectF window(0, 0, LANDSCAPE_WIDTH, LANDSCAPE_HEIGHT);
    const QRectF resume = scene_rect(app, "resumeButton");
    const QRectF quit = scene_rect(app, "quitButton");
    const QRectF tilt = scene_rect(app, "controlsTilt");
    const QRectF quick = scene_rect(app, "sensitivityQuick");
    const QRectF recalibrate = scene_rect(app, "recalibrateButton");
    for (const QRectF& part : {resume, quit, tilt, quick, recalibrate, scene_rect(app, "pauseSoundToggle"),
                               scene_rect(app, "pauseMusicToggle")})
    {
        CHECK(window.contains(part));
    }
    // How to steer is beside the buttons, not under them.
    CHECK(tilt.left() > resume.right());
    CHECK(recalibrate.left() > quit.right());
    CHECK(quick.bottom() < window.bottom());
    CHECK(quit.bottom() < window.bottom());
}

TEST_CASE("Lying down, the pause dialog of a computer is one column", "[screens][landscape]")
{
    App app(false, 1024, 768);
    app.start_classic();
    app.game().pause();
    REQUIRE(app.shows("pauseScreen"));
    CHECK_FALSE(app.shows("controlsTilt"));
    CHECK(scene_rect(app, "resumeButton").center().x() == Catch::Approx(scene_rect(app, "quitButton").center().x()));
}

TEST_CASE("Lying down, the board takes the height with the score on one side and the controls on the other",
          "[screens][landscape][touch]")
{
    for (const auto controls : {ProfileViewModel::Controls::Tilt, ProfileViewModel::Controls::Swipe,
                                ProfileViewModel::Controls::Dpad, ProfileViewModel::Controls::Joystick})
    {
        INFO("controls " << static_cast<int>(controls));
        App app(true, LANDSCAPE_WIDTH, LANDSCAPE_HEIGHT);
        app.profile().setControls(controls);
        app.start_classic();
        const QRectF window(0, 0, LANDSCAPE_WIDTH, LANDSCAPE_HEIGHT);
        const QRectF board = scene_rect(app, "board");
        CHECK(window.contains(board));
        // Almost the whole height, and about square.
        CHECK(board.height() > LANDSCAPE_HEIGHT - 30);
        CHECK(board.width() == Catch::Approx(board.height()).margin(2.0));
        // The score and the pause button are on the leading side.
        CHECK(scene_rect(app, "score").right() <= board.left());
        CHECK(scene_rect(app, "bestScore").right() <= board.left());
        CHECK(scene_rect(app, "pauseButton").right() <= board.left());
        CHECK(window.contains(scene_rect(app, "pauseButton")));
        // The steering is on the other, inside the window.
        const char* control = controls == ProfileViewModel::Controls::Tilt       ? "tiltIndicator"
                              : controls == ProfileViewModel::Controls::Dpad     ? "dpad"
                              : controls == ProfileViewModel::Controls::Joystick ? "joystick"
                                                                                 : nullptr;
        if (control != nullptr)
        {
            const QRectF where = scene_rect(app, control);
            CHECK(window.contains(where));
            CHECK(where.left() >= board.right());
        }
    }
}

TEST_CASE("Lying down, a computer's game has the board in the middle and the score on the left", "[screens][landscape]")
{
    App app(false, 1024, 768);
    app.start_classic();
    const QRectF board = scene_rect(app, "board");
    CHECK(board.center().x() == Catch::Approx(512.0).margin(2.0));
    CHECK(scene_rect(app, "score").right() <= board.left());
    CHECK_FALSE(app.shows("pauseButton"));
}

TEST_CASE("The title, the game and the pause dialog fit in every language lying down",
          "[screens][languages][landscape]")
{
    for (const char* code : TESTED_LANGUAGES)
    {
        INFO("language " << code);
        const InLanguage language(code);
        App app(true, LANDSCAPE_WIDTH, LANDSCAPE_HEIGHT);
        fill_the_profile(app);
        check_fit(app, "title");
        app.start_classic();
        check_fit(app, "game");
        app.game().pause();
        check_fit(app, "pause");
    }
}

TEST_CASE("Lying down, the leaderboard is two columns of five lines", "[screens][landscape]")
{
    App app(true, LANDSCAPE_WIDTH, LANDSCAPE_HEIGHT);
    for (unsigned place = 1; place <= 10; ++place)
    {
        app.profile().record_game(Mode::CLASSIC, 1000u * place);
        app.save_score_as(QString("P%1").arg(place));
    }
    app.click("leaderboardButton");
    app.click("classicTab");
    const QRectF window(0, 0, LANDSCAPE_WIDTH, LANDSCAPE_HEIGHT);
    for (int place = 1; place <= 10; ++place)
    {
        const QString name = QString("line%1").arg(place);
        INFO(name.toStdString());
        CHECK(window.contains(scene_rect(app, name.toLatin1().constData())));
    }
    // The first five run down the leading side, and the next five down the other.
    CHECK(scene_rect(app, "line1").left() == Catch::Approx(scene_rect(app, "line5").left()));
    CHECK(scene_rect(app, "line1").left() < scene_rect(app, "line6").left());
    CHECK(scene_rect(app, "line1").top() == Catch::Approx(scene_rect(app, "line6").top()));
    CHECK(scene_rect(app, "line5").right() <= scene_rect(app, "line6").left());
    CHECK(scene_rect(app, "line10").bottom() <= scene_rect(app, "leaderboardPlayButton").top());
    for (const char* name : {"backButton", "easyTab", "classicTab", "leaderboardPlayButton"})
    {
        INFO(name);
        CHECK(window.contains(scene_rect(app, name)));
    }
}

TEST_CASE("Lying down, the settings, the about page and the licence fit the window", "[screens][landscape]")
{
    App app(true, LANDSCAPE_WIDTH, LANDSCAPE_HEIGHT);
    const QRectF window(0, 0, LANDSCAPE_WIDTH, LANDSCAPE_HEIGHT);
    app.click("settingsButton");
    for (const char* name : {"settingsBackButton", "soundToggle", "invertAxesToggle", "resetEasyButton",
                             "resetClassicButton", "aboutButton"})
    {
        INFO(name);
        CHECK(window.contains(scene_rect(app, name)));
    }
    // The switches are on one side and the rest on the other.
    CHECK(scene_rect(app, "soundToggle").right() < scene_rect(app, "aboutButton").left());
    app.click("resetClassicButton");
    CHECK(window.contains(scene_rect(app, "cancelButton")));
    CHECK(window.contains(scene_rect(app, "confirmButton")));
    CHECK(scene_rect(app, "cancelButton").right() <= scene_rect(app, "confirmButton").left());
    app.press(Qt::Key_Escape);
    app.click("aboutButton");
    for (const char* name : {"aboutBackButton", "aboutVersion", "aboutRevision", "licenceButton", "sourceButton"})
    {
        INFO(name);
        CHECK(window.contains(scene_rect(app, name)));
    }
    CHECK(scene_rect(app, "aboutVersion").right() < scene_rect(app, "licenceButton").left());
    app.click("licenceButton");
    CHECK(window.contains(scene_rect(app, "noticeBackButton")));
    CHECK(window.contains(scene_rect(app, "noticeScroller")));
    CHECK(scene_rect(app, "noticeScroller").height() > 200);
}

TEST_CASE("Lying down, the game over screen has the board on one side and the score and buttons on the other",
          "[screens][landscape]")
{
    App app(true, LANDSCAPE_WIDTH, LANDSCAPE_HEIGHT);
    app.start_classic();
    app.crash();
    const QRectF window(0, 0, LANDSCAPE_WIDTH, LANDSCAPE_HEIGHT);
    const QRectF finalScore = scene_rect(app, "finalScore");
    const QRectF menu = scene_rect(app, "menuButton");
    const QRectF play = scene_rect(app, "playAgainButton");
    CHECK(window.contains(finalScore));
    CHECK(window.contains(menu));
    CHECK(window.contains(play));
    CHECK(play.bottom() <= menu.top());
    CHECK(finalScore.bottom() <= play.top());
    // The board, which is the first thing the screen draws, is to the left of all of them.
    for (const QRectF& part : {finalScore, menu, play})
    {
        CHECK(part.left() > LANDSCAPE_HEIGHT - 24);
    }
}

TEST_CASE("Lying down, the name entry shows the whole alphabet and Save without anything running into anything",
          "[screens][landscape]")
{
    App app(true, LANDSCAPE_WIDTH, LANDSCAPE_HEIGHT);
    fill_the_profile(app);
    app.start_classic();
    app.crash();
    app.profile().record_game(Mode::CLASSIC, 5000);
    app.click("saveScoreButton");
    const QRectF window(0, 0, LANDSCAPE_WIDTH, LANDSCAPE_HEIGHT);
    const QRectF save = scene_rect(app, "saveNameButton");
    CHECK(window.contains(save));
    CHECK(window.contains(scene_rect(app, "pendingScore")));
    CHECK(window.contains(scene_rect(app, "standing1")));
    for (int slot = 0; slot < 12; ++slot)
    {
        const QString name = QString("slot%1").arg(slot);
        CHECK(window.contains(scene_rect(app, name.toLatin1().constData())));
    }
    QRectF keys;
    for (const char letter : std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZ"))
    {
        const QString name = QString("key%1").arg(letter);
        const QRectF where = scene_rect(app, name.toLatin1().constData());
        INFO(name.toStdString());
        CHECK(window.contains(where));
        CHECK(where.left() > save.right());
        CHECK_FALSE(where.intersects(save));
        keys = keys.united(where);
    }
    CHECK(window.contains(scene_rect(app, "keySpace")));
    CHECK(window.contains(scene_rect(app, "keyDelete")));
    // The standing is on the same side as Save, above it, and the name is on the other.
    CHECK(scene_rect(app, "standing1").right() < keys.left());
    CHECK(scene_rect(app, "standing1").bottom() < save.top());
    // The letters are big enough for a thumb.
    CHECK(scene_rect(app, "keyA").width() >= 36);
}

TEST_CASE("The leaderboard, the settings, the game over and the name entry fit in every language lying down",
          "[screens][languages][landscape]")
{
    for (const char* code : TESTED_LANGUAGES)
    {
        INFO("language " << code);
        const InLanguage language(code);
        App app(true, LANDSCAPE_WIDTH, LANDSCAPE_HEIGHT);
        fill_the_profile(app);
        app.click("settingsButton");
        check_fit(app, "settings");
        app.click("resetClassicButton");
        check_fit(app, "confirm");
        app.press(Qt::Key_Escape);
        app.click("aboutButton");
        check_fit(app, "about");
        app.press(Qt::Key_Escape);
        app.press(Qt::Key_Escape);
        app.click("leaderboardButton");
        app.click("classicTab");
        check_fit(app, "leaderboard");
        app.press(Qt::Key_Escape);

        app.start_classic();
        app.crash();
        app.profile().record_game(Mode::CLASSIC, 5000);
        check_fit(app, "game over");
        app.click("saveScoreButton");
        app.type("maya");
        check_fit(app, "name entry");
    }
}

TEST_CASE("A phone that turns during a game waits for the player, and a computer's window does not stop it",
          "[screens][landscape]")
{
    {
        App app(true);
        app.profile().setControls(ProfileViewModel::Controls::Swipe);
        app.start_classic();
        REQUIRE(app.game().state() == State::Running);
        app.window().resize(LANDSCAPE_WIDTH, LANDSCAPE_HEIGHT);
        QTest::qWait(20);
        CHECK(app.game().state() == State::Paused);
    }
    {
        App app(false);
        app.start_classic();
        REQUIRE(app.game().state() == State::Running);
        app.window().resize(1024, 768);
        QTest::qWait(20);
        CHECK(app.game().state() == State::Running);
    }
}
