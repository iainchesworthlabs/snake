// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/input/action.hpp"
#include "snake/sim/session.hpp"
#include "snake/sim/tick_pacer.hpp"

#include <QtQml/qqmlregistration.h>

#include <QObject>
#include <QString>
#include <cstdint>
#include <functional>
#include <optional>

namespace snake::ui
{

/// What QML sees of the game: the state, the score and the mode, and the controls. It owns one sim::Session and a
/// sim::TickPacer. The front end calls advance() once a frame with the time since the last frame, and the view-model
/// runs the ticks that are due. It holds no rules; they belong in libs/sim. The board item reads the cells from
/// session() and redraws on boardChanged.
class GameViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    /// Where the game is. The user interface chooses what to show for each state.
    Q_PROPERTY(State state READ state NOTIFY stateChanged FINAL)
    /// The score as six digits with leading zeros, such as "000420".
    Q_PROPERTY(QString score READ score NOTIFY scoreChanged FINAL)
    /// The score as a number, for the profile to rank and compare.
    Q_PROPERTY(int points READ points NOTIFY scoreChanged FINAL)
    /// The game plays itself, as the title screen shows it. Games then take their seeds from a sequence of the demo's
    /// own, so a real game's seeds are never used up, and a game that ends starts again by itself. Setting it starts
    /// a new game.
    Q_PROPERTY(bool autoplay READ autoplay WRITE setAutoplay NOTIFY autoplayChanged FINAL)
    /// The row of the board the head is in, from 0 at the top. The title's preview scrolls to keep it in view.
    Q_PROPERTY(int headRow READ headRow NOTIFY boardChanged FINAL)
    /// The mode of the current game. Setting it starts a new game in that mode.
    Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY modeChanged FINAL)

public:
    enum class State : std::uint8_t
    {
        NotStarted,
        Running,
        Paused,
        GameOver,
    };
    Q_ENUM(State)

    enum class Mode : std::uint8_t
    {
        Easy,
        Classic,
    };
    Q_ENUM(Mode)

    /// What a tick did besides move the snake. The sound and the effects react to these.
    enum class Event : std::uint8_t
    {
        Ate,      // the snake ate the food
        AteBonus, // the snake ate the bonus item
        Crashed,  // the snake hit the wall or itself and the game ended
        Won,      // the snake ate the last food and filled the board
    };
    Q_ENUM(Event)

    /// The event that a tick with `outcome` is, or nothing when the snake only moved or the game was not running.
    [[nodiscard]] static std::optional<Event> event_of(sim::TickOutcome outcome) noexcept;

    /// Gives the seed of each new game.
    using SeedSource = std::function<std::uint64_t()>;

    /// Games are seeded from the system's random number generator.
    explicit GameViewModel(QObject* parent = nullptr);
    /// Tests give the seeds.
    explicit GameViewModel(SeedSource seeds, QObject* parent = nullptr);

    [[nodiscard]] State state() const;
    [[nodiscard]] QString score() const;
    [[nodiscard]] int points() const;
    [[nodiscard]] Mode mode() const;
    void setMode(Mode mode);

    [[nodiscard]] bool autoplay() const noexcept
    {
        return m_Autoplay;
    }
    void setAutoplay(bool on);

    [[nodiscard]] int headRow() const;

    /// The seed of the first game the demo plays. Each game after it takes the next number.
    static constexpr std::uint64_t DEMO_SEED = 0x5EED5EED;

    [[nodiscard]] const sim::Session& session() const noexcept
    {
        return m_Session;
    }

    /// How far the game is from the last tick to the next, from 0 to 1. The board uses it to draw the snake between
    /// cells.
    [[nodiscard]] double tickProgress() const;

    /// A new game in the current mode, waiting to start.
    Q_INVOKABLE void newGame();
    /// A new game in the current mode, started at once.
    Q_INVOKABLE void restart();
    /// NOT_STARTED to running.
    Q_INVOKABLE void start();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();

    /// Time has passed: `seconds` since the last call, as the frame animation reports it. Runs the ticks that are due
    /// while the game is running; at any other time it does nothing. A negative or not-a-number time is ignored.
    Q_INVOKABLE void advance(double seconds);

    /// A key went down. `qtKey` is a value of Qt::Key. Returns true when the game uses the key, so the caller can
    /// accept the event. A held key's repeats are used up without effect.
    Q_INVOKABLE bool handleKey(int qtKey, bool autoRepeat);

    /// Steers the snake, as an arrow key does: tilt, a swipe and the d-pad come here. Returns false when the input is
    /// dropped (the game is not running, or the turn is a reversal or a repeat).
    bool turn(sim::Direction direction);

signals:
    /// A tick ate, crashed or won. It is sent as the tick happens, before the state changes.
    void tickEvent(snake::ui::GameViewModel::Event event);
    /// The state changed.
    void stateChanged();
    /// A new game began running, from the title or by restarting: not a resume from a pause.
    void gameStarted();
    void autoplayChanged();
    void scoreChanged();
    void modeChanged();
    /// The board is different: a tick ran or a new game began.
    void boardChanged();
    /// Time has passed within a tick, so the snake is further on between its cells.
    void progressChanged();

private:
    void perform(input::Action action);
    /// Tells QML what changed since the state and score were `state_before` and `score_before`, and that the board did
    /// when `board_changed`.
    void publish(sim::GameState state_before, std::uint32_t score_before, bool board_changed);

    /// The seed of the next game: the demo's own sequence while it plays itself, else from the seed source.
    [[nodiscard]] std::uint64_t next_seed();

    SeedSource m_Seeds;
    bool m_Autoplay = false;
    std::uint64_t m_DemoSeed = DEMO_SEED;
    sim::Session m_Session;
    sim::TickPacer m_Pacer;
};

} // namespace snake::ui
