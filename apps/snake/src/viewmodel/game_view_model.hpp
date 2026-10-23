// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/game.hpp"
#include "snake/logger.hpp"

#include <QtQml/qqmlregistration.h>

#include <QObject>
#include <QString>
#include <QTimer>
#include <chrono>
#include <memory>

namespace snake::ui
{

/// What QML sees of the game: the state and the frame counter, and the controls. A timer ticks the
/// game at the frame rate while it runs. This class holds no rules; they belong in snake::Game.
class GameViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    /// The number of frames ticked so far.
    Q_PROPERTY(qint64 frame READ frame NOTIFY frameChanged FINAL)
    /// One of "ready", "running", "paused" and "game over". The user interface translates it.
    Q_PROPERTY(QString state READ state NOTIFY stateChanged FINAL)

public:
    /// Logs to standard error at info level and ticks at snake::TimingLoop::frame_interval().
    explicit GameViewModel(QObject* parent = nullptr);
    /// The logger and the tick interval are given by tests.
    GameViewModel(std::shared_ptr<const Logger> logger, std::chrono::milliseconds interval, QObject* parent = nullptr);

    [[nodiscard]] qint64 frame() const;
    [[nodiscard]] QString state() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void end();

signals:
    /// Emitted once per counted frame.
    void frameChanged();
    /// Emitted once per change of state.
    void stateChanged();

private:
    void tick();

    Game m_Game;
    QTimer m_Timer;
};

} // namespace snake::ui
