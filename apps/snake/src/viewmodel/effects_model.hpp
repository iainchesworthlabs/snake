// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "board/effects.hpp"
#include "viewmodel/game_view_model.hpp"

#include <QtQml/qqmlregistration.h>

#include <QObject>
#include <cstdint>

namespace snake::ui
{

/// The show of the game for QML: it watches a GameViewModel and starts the effects when the snake eats or crashes. The
/// reduced-motion setting is given to it by a binding. The frame animation of Main.qml calls advance() while it
/// is active; the board item draws what effects() holds.
class EffectsModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(snake::ui::GameViewModel* game READ game WRITE setGame NOTIFY gameChanged FINAL)
    /// Reduced motion: no shake.
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged FINAL)
    /// Something is moving and the game is not paused, so the frame animation should run.
    Q_PROPERTY(bool active READ active NOTIFY activeChanged FINAL)

public:
    explicit EffectsModel(QObject* parent = nullptr);

    [[nodiscard]] GameViewModel* game() const noexcept
    {
        return m_Game;
    }
    void setGame(GameViewModel* game);

    [[nodiscard]] bool reducedMotion() const noexcept
    {
        return m_Effects.reduced_motion();
    }
    void setReducedMotion(bool reduced);

    [[nodiscard]] bool active() const noexcept
    {
        return m_Active;
    }

    [[nodiscard]] const effects::Effects& effects() const noexcept
    {
        return m_Effects;
    }

    /// Time has passed: `seconds` since the last frame.
    Q_INVOKABLE void advance(double seconds);

signals:
    void gameChanged();
    void reducedMotionChanged();
    void activeChanged();
    /// Something moved: the board item draws again.
    void changed();

private:
    void on_tick_event(GameViewModel::Event event);
    void on_state_changed();
    /// Tells QML when `active` is different now.
    void update_active();

    GameViewModel* m_Game = nullptr;
    effects::Effects m_Effects;
    /// The score at the last event, to know how much the food was worth.
    std::int64_t m_LastPoints = 0;
    bool m_Active = false;
};

} // namespace snake::ui
