// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "platform/sound_sink.hpp"
#include "snake/audio/tunes.hpp"
#include "viewmodel/game_view_model.hpp"
#include "viewmodel/profile_view_model.hpp"

#include <QtQml/qqmlregistration.h>

#include <QObject>
#include <optional>

namespace snake::ui
{

/// Decides what is heard when. It watches the game and the profile and tells a SoundSink: an effect for eating, the
/// bonus item, a crash, a game starting, a button pressed and a new best score, and the tune for the title and for a
/// running game, which stops while the game is paused or over. It follows the sound and music settings as they change.
/// It holds no sound itself, and does nothing without a sink.
class SoundDirector : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(snake::ui::GameViewModel* game READ game WRITE setGame NOTIFY gameChanged FINAL)
    Q_PROPERTY(snake::ui::ProfileViewModel* profile READ profile WRITE setProfile NOTIFY profileChanged FINAL)
    /// The program is the one the person is using. When it is put away (the phone's home button, a call, another
    /// program in front) the music stops and no effect sounds; when it comes back the music for the moment returns.
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged FINAL)

public:
    /// Plays on the program's default sink (see default_sound_sink()).
    explicit SoundDirector(QObject* parent = nullptr);
    /// Plays on `sink`, which must outlive the director. Tests give a recording one.
    explicit SoundDirector(SoundSink* sink, QObject* parent = nullptr);

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

    [[nodiscard]] bool active() const noexcept
    {
        return m_Active;
    }
    void setActive(bool active);

signals:
    void gameChanged();
    void profileChanged();
    void activeChanged();

private:
    void on_tick_event(GameViewModel::Event event);
    void on_state_changed();
    void on_result_changed();
    void on_settings_changed();
    void on_button_pressed();

    [[nodiscard]] bool sound_on() const;
    [[nodiscard]] bool music_on() const;
    void play(audio::Effect effect);
    /// The tune the game is in the mood for, or none.
    [[nodiscard]] std::optional<audio::Tune> tune_wanted() const;
    /// Makes the music what it should be now: the tune wanted if music is on, or silence.
    void update_music();

    SoundSink* m_Sink = nullptr;
    GameViewModel* m_Game = nullptr;
    ProfileViewModel* m_Profile = nullptr;
    bool m_Active = true;
    GameViewModel::State m_State = GameViewModel::State::NotStarted;
    /// The tune the sink was last told to play, so that a state signal that changes nothing does not restart it.
    std::optional<audio::Tune> m_Tune;
};

} // namespace snake::ui
