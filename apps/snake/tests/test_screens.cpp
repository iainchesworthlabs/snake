// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/mode.hpp"
#include "theme/tokens.hpp"
#include "viewmodel/game_view_model.hpp"
#include "viewmodel/profile_view_model.hpp"

#include <QtTest/QTest>

#include <catch2/catch_test_macros.hpp>

#include <QColor>
#include <QImage>
#include <QLocale>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QString>
#include <QVariantMap>
#include <Qt>
#include <memory>

using snake::sim::Mode;
using snake::ui::GameViewModel;
using snake::ui::ProfileViewModel;
using State = GameViewModel::State;
namespace tokens = snake::ui::tokens;

namespace
{

constexpr int PAGE_WIDTH = 390;
constexpr int PAGE_HEIGHT = 844;

// The window is the size of the design's page, so a screen is drawn at the scale of the design and its measures can be
// read off the pixels.
class App
{
public:
    App()
    {
        QQmlComponent component(&m_Engine, "SnakeApp", "Main");
        INFO(component.errorString().toStdString());
        REQUIRE(component.isReady());
        m_Root.reset(
            component.createWithInitialProperties({{"windowWidth", PAGE_WIDTH}, {"windowHeight", PAGE_HEIGHT}}));
        m_Window = qobject_cast<QQuickWindow*>(m_Root.get());
        REQUIRE(m_Window != nullptr);
        m_Window->show();
        REQUIRE(QTest::qWaitForWindowExposed(m_Window));
        m_Window->requestActivate();
        REQUIRE(QTest::qWaitForWindowActive(m_Window));
        m_Game = m_Window->findChild<GameViewModel*>("game");
        m_Profile = m_Window->findChild<ProfileViewModel*>("profile");
        REQUIRE(m_Game != nullptr);
        REQUIRE(m_Profile != nullptr);
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

    /// Chooses Classic and starts the game.
    void start_classic()
    {
        click("modeCard1");
        click("playButton");
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
    ProfileViewModel* m_Profile = nullptr;
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
    app.press(Qt::Key_Down);
    CHECK(app.focus_name() == "quitButton");
    app.press(Qt::Key_Up);
    CHECK(app.focus_name() == "restartButton");
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
