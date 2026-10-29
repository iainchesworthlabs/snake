// SPDX-License-Identifier: GPL-3.0-or-later
#include "viewmodel/sound_director.hpp"

#include "platform/sound_sink.hpp"
#include "snake/audio/tunes.hpp"
#include "viewmodel/game_view_model.hpp"
#include "viewmodel/profile_view_model.hpp"
#include "viewmodel/ui_events.hpp"

#include <optional>

namespace snake::ui
{

SoundDirector::SoundDirector(QObject* parent) : SoundDirector(default_sound_sink(), parent) {}

SoundDirector::SoundDirector(SoundSink* sink, QObject* parent) : QObject(parent), m_Sink(sink)
{
    connect(&UiEvents::instance(), &UiEvents::buttonPressed, this, &SoundDirector::on_button_pressed);
}

void SoundDirector::setGame(GameViewModel* game)
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
        connect(m_Game, &GameViewModel::tickEvent, this, &SoundDirector::on_tick_event);
        connect(m_Game, &GameViewModel::stateChanged, this, &SoundDirector::on_state_changed);
        m_State = m_Game->state();
    }
    update_music();
    emit gameChanged();
}

void SoundDirector::setProfile(ProfileViewModel* profile)
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
        connect(m_Profile, &ProfileViewModel::resultChanged, this, &SoundDirector::on_result_changed);
        connect(m_Profile, &ProfileViewModel::settingsChanged, this, &SoundDirector::on_settings_changed);
    }
    send_volume();
    update_music();
    emit profileChanged();
}

void SoundDirector::setActive(bool active)
{
    if (active == m_Active)
    {
        return;
    }
    m_Active = active;
    update_music();
    emit activeChanged();
}

bool SoundDirector::sound_on() const
{
    return m_Active && (m_Profile == nullptr || m_Profile->sound());
}

bool SoundDirector::music_on() const
{
    return m_Active && (m_Profile == nullptr || m_Profile->music());
}

void SoundDirector::play(audio::Effect effect)
{
    if (m_Sink != nullptr && sound_on())
    {
        m_Sink->play(effect);
    }
}

std::optional<audio::Tune> SoundDirector::tune_wanted() const
{
    if (m_Game == nullptr)
    {
        return std::nullopt;
    }
    switch (m_State)
    {
    case GameViewModel::State::NotStarted:
        return audio::Tune::TITLE;
    case GameViewModel::State::Running:
        return audio::Tune::GAME;
    case GameViewModel::State::Paused:
    case GameViewModel::State::GameOver:
        break;
    }
    return std::nullopt;
}

void SoundDirector::update_music()
{
    if (m_Sink == nullptr)
    {
        return;
    }
    const std::optional<audio::Tune> wanted = music_on() ? tune_wanted() : std::nullopt;
    if (wanted == m_Tune)
    {
        return;
    }
    m_Tune = wanted;
    if (wanted)
    {
        m_Sink->play_music(*wanted);
    }
    else
    {
        m_Sink->stop_music();
    }
}

void SoundDirector::on_tick_event(GameViewModel::Event event)
{
    switch (event)
    {
    case GameViewModel::Event::Ate:
        play(audio::Effect::EAT);
        break;
    case GameViewModel::Event::AteBonus:
        play(audio::Effect::BONUS);
        break;
    case GameViewModel::Event::Crashed:
        play(audio::Effect::CRASH);
        break;
    case GameViewModel::Event::Won:
        play(audio::Effect::NEW_BEST);
        break;
    }
}

void SoundDirector::on_state_changed()
{
    const GameViewModel::State before = m_State;
    m_State = m_Game->state();
    // A game begins from the title or from the end of another, not from a pause.
    if (m_State == GameViewModel::State::Running && before != GameViewModel::State::Paused &&
        before != GameViewModel::State::Running)
    {
        play(audio::Effect::START);
    }
    update_music();
}

void SoundDirector::on_result_changed()
{
    if (m_Profile->newBest())
    {
        play(audio::Effect::NEW_BEST);
    }
}

void SoundDirector::on_settings_changed()
{
    send_volume();
    update_music();
}

void SoundDirector::send_volume()
{
    if (m_Sink == nullptr || m_Profile == nullptr)
    {
        return;
    }
    // The ear hears loudness on a curve, so the steps of the setting are squared to sound even.
    const float step = static_cast<float>(m_Profile->volume()) / static_cast<float>(ProfileViewModel::volumeMax());
    m_Sink->set_volume(step * step);
}

void SoundDirector::on_button_pressed()
{
    play(audio::Effect::SELECT);
}

} // namespace snake::ui
