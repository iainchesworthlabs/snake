// SPDX-License-Identifier: GPL-3.0-or-later
#include "viewmodel/profile_view_model.hpp"

#include "platform/motion_preference.hpp"
#include "platform/save_file.hpp"
#include "snake/profile/leaderboard.hpp"
#include "snake/profile/player_name.hpp"
#include "snake/profile/unlocks.hpp"
#include "snake/sim/mode.hpp"
#include "snake/sim/scoring.hpp"
#include "viewmodel/game_view_model.hpp"

#include <QDateTime>
#include <QDebug>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace snake::ui
{

namespace
{

constexpr ModeListModel::Entry entry_of(profile::Unlockable unlockable) noexcept
{
    return unlockable == profile::Unlockable::PORTALS ? ModeListModel::Entry::Portals : ModeListModel::Entry::HotPotato;
}

QString six_digits(std::uint32_t score)
{
    const auto digits = sim::score_digits(score);
    return QString::fromLatin1(digits.data(), static_cast<qsizetype>(digits.size()));
}

QString name_text(const profile::PlayerName& name)
{
    return QString::fromLatin1(name.view().data(), static_cast<qsizetype>(name.view().size()));
}

QVariantMap line(std::size_t place, const profile::PlayerName& name, std::uint32_t score, bool mine)
{
    return {{QStringLiteral("place"), static_cast<int>(place)},
            {QStringLiteral("name"), name_text(name)},
            {QStringLiteral("score"), six_digits(score)},
            {QStringLiteral("mine"), mine}};
}

} // namespace

ProfileViewModel::ProfileViewModel(QObject* parent)
    : ProfileViewModel([] { return static_cast<std::int64_t>(QDateTime::currentSecsSinceEpoch()); }, parent)
{
}

ProfileViewModel::ProfileViewModel(Clock clock, QObject* parent)
    : ProfileViewModel(std::move(clock), &system_prefers_reduced_motion, parent)
{
}

ProfileViewModel::ProfileViewModel(Clock clock, MotionPreference motion_preference, QObject* parent)
    : QObject(parent), m_Clock(std::move(clock)), m_MotionPreference(std::move(motion_preference)),
      m_Modes(m_Data.unlocks, this), m_Board(m_Data.leaderboards, this)
{
}

void ProfileViewModel::setGame(GameViewModel* game)
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
        connect(m_Game, &GameViewModel::stateChanged, this, &ProfileViewModel::on_state_changed);
        connect(m_Game, &GameViewModel::modeChanged, this, &ProfileViewModel::bestChanged);
    }
    emit gameChanged();
    emit bestChanged();
}

void ProfileViewModel::setBoardMode(GameViewModel::Mode mode)
{
    if (mode == m_BoardMode)
    {
        return;
    }
    m_BoardMode = mode;
    m_Board.show(to_sim(mode));
    emit boardModeChanged();
}

QString ProfileViewModel::best() const
{
    if (m_Game == nullptr)
    {
        return six_digits(0);
    }
    return six_digits(best_of(to_sim(m_Game->mode())));
}

std::uint32_t ProfileViewModel::best_of(sim::Mode mode) const noexcept
{
    const std::uint32_t saved = m_Data.leaderboards.of(mode).best_score();
    return m_Pending && m_Pending->mode == mode ? std::max(saved, m_Pending->score) : saved;
}

void ProfileViewModel::on_state_changed()
{
    if (m_Game->state() == GameViewModel::State::GameOver)
    {
        record_game(to_sim(m_Game->mode()), static_cast<std::uint32_t>(m_Game->points()));
    }
}

void ProfileViewModel::record_game(sim::Mode mode, std::uint32_t score)
{
    // The score of the board before this game: a game that was never saved does not count.
    const profile::Leaderboard& board = m_Data.leaderboards.of(mode);
    const std::uint32_t best = board.best_score();

    m_NewBest = score > best;
    if (m_NewBest)
    {
        m_PointsToBest = 0;
    }
    else
    {
        m_PointsToBest = best == 0 ? -1 : static_cast<int>(best - score);
    }

    m_UnlockedNow.clear();
    const profile::Unlocks opened = m_Data.unlocks.record_score(mode, score);
    for (const auto unlockable : {profile::Unlockable::PORTALS, profile::Unlockable::HOT_POTATO})
    {
        if (opened.has(unlockable))
        {
            m_UnlockedNow.push_back(ModeListModel::name_of(entry_of(unlockable)));
        }
    }
    if (!opened.none())
    {
        m_Modes.refresh();
    }

    const std::optional<std::size_t> place = board.rank_for(score);
    set_pending(place ? std::optional(Pending{.mode = mode, .score = score, .unix_seconds = m_Clock(), .place = *place})
                      : std::nullopt);
    write();

    emit bestChanged();
    emit resultChanged();
}

QString ProfileViewModel::saveFile() const
{
    return m_File ? m_File->path() : QString();
}

void ProfileViewModel::setSaveFile(const QString& path)
{
    if (path == saveFile())
    {
        return;
    }
    m_File.reset();
    m_Writing = false;
    m_LoadStatus = QStringLiteral("no save file (kept in memory)");
    m_WriteStatus = QStringLiteral("nothing written yet");
    if (!path.isEmpty())
    {
        m_File = std::make_unique<SaveFile>(path);
        const auto loaded = m_File->load();
        if (loaded)
        {
            m_Data = *loaded;
            m_Writing = true;
            m_LoadStatus = QStringLiteral("loaded, %1 + %2 scores")
                               .arg(m_Data.leaderboards.of(sim::Mode::EASY).size())
                               .arg(m_Data.leaderboards.of(sim::Mode::CLASSIC).size() +
                                    m_Data.leaderboards.of(sim::Mode::PORTALS).size() +
                                    m_Data.leaderboards.of(sim::Mode::HOT_POTATO).size());
        }
        else
        {
            switch (loaded.error())
            {
            case SaveFile::Error::MISSING:
                // A first run: the game starts as the system asks for motion.
                m_Data.settings.reduced_motion = m_MotionPreference();
                m_Writing = true;
                m_LoadStatus = QStringLiteral("no file yet (a first run)");
                break;
            case SaveFile::Error::DAMAGED:
                qWarning() << "The save file is damaged. It was moved to" << m_File->damaged_path()
                           << "and the game starts afresh.";
                m_Data = profile::SaveData{};
                m_Data.settings.reduced_motion = m_MotionPreference();
                m_Writing = true;
                m_LoadStatus = QStringLiteral("the file was damaged and was set aside; started afresh");
                break;
            case SaveFile::Error::NEWER_VERSION:
                qWarning() << "The save file was written by a newer version of the game and is left alone:" << path;
                m_LoadStatus = QStringLiteral("the file is from a newer version: NOT WRITING");
                break;
            case SaveFile::Error::UNREADABLE:
                qWarning() << "The save file could not be read and is left alone:" << path;
                m_LoadStatus = QStringLiteral("the file could not be read: NOT WRITING");
                break;
            }
        }
    }
    m_Modes.refresh();
    m_Board.mark_latest(std::nullopt, 0);
    m_Board.refresh();
    set_pending(std::nullopt);
    emit saveFileChanged();
    emit settingsChanged();
    emit bestChanged();
    emit recentNamesChanged();
}

void ProfileViewModel::write()
{
    if (!m_Writing)
    {
        return;
    }
    if (!m_File->save(m_Data))
    {
        qWarning() << "The save file could not be written:" << m_File->path();
        m_WriteStatus = QStringLiteral("the last write FAILED");
    }
    else
    {
        m_WriteStatus = QStringLiteral("last write ok");
    }
    emit saveFileChanged();
}

QString ProfileViewModel::saveStatus() const
{
    const QString where = m_File ? m_File->path() : QString();
    return QStringLiteral("%1; %2; %3").arg(m_LoadStatus, m_WriteStatus, where.isEmpty() ? QStringLiteral("-") : where);
}

template <typename Change>
void ProfileViewModel::change_settings(Change change)
{
    const profile::Settings before = m_Data.settings;
    change(m_Data.settings);
    if (m_Data.settings == before)
    {
        return;
    }
    write();
    emit settingsChanged();
}

ProfileViewModel::Controls ProfileViewModel::controls() const noexcept
{
    return static_cast<Controls>(m_Data.settings.controls);
}

void ProfileViewModel::setControls(Controls controls)
{
    change_settings([controls](profile::Settings& settings)
                    { settings.controls = static_cast<profile::ControlScheme>(controls); });
}

ProfileViewModel::TiltSensitivity ProfileViewModel::tiltSensitivity() const noexcept
{
    return static_cast<TiltSensitivity>(m_Data.settings.tilt_sensitivity);
}

void ProfileViewModel::setTiltSensitivity(TiltSensitivity sensitivity)
{
    change_settings([sensitivity](profile::Settings& settings)
                    { settings.tilt_sensitivity = static_cast<profile::TiltSensitivity>(sensitivity); });
}

void ProfileViewModel::setInvertAxes(bool invert)
{
    change_settings([invert](profile::Settings& settings) { settings.invert_axes = invert; });
}

void ProfileViewModel::setSound(bool on)
{
    change_settings([on](profile::Settings& settings) { settings.sound = on; });
}

void ProfileViewModel::setMusic(bool on)
{
    change_settings([on](profile::Settings& settings) { settings.music = on; });
}

void ProfileViewModel::setVolume(int level)
{
    const auto clamped = static_cast<std::uint8_t>(std::clamp(level, 0, volumeMax()));
    change_settings([clamped](profile::Settings& settings) { settings.volume = clamped; });
}

void ProfileViewModel::setReducedMotion(bool reduced)
{
    change_settings([reduced](profile::Settings& settings) { settings.reduced_motion = reduced; });
}

void ProfileViewModel::resetLeaderboard(GameViewModel::Mode mode)
{
    const sim::Mode cleared = to_sim(mode);
    m_Data.leaderboards.of(cleared).clear();
    // A score that waited for a place on this board has lost the place it was given.
    if (m_Pending && m_Pending->mode == cleared)
    {
        set_pending(std::nullopt);
    }
    m_Board.mark_latest(std::nullopt, 0);
    m_Board.refresh();
    write();
    emit bestChanged();
}

void ProfileViewModel::calibrateTilt()
{
    emit calibrationRequested();
}

void ProfileViewModel::set_pending(const std::optional<Pending>& pending)
{
    m_Pending = pending;
    m_Name.clear();
    emit pendingChanged();
    emit entryChanged();
    emit standingChanged();
}

QString ProfileViewModel::pendingScore() const
{
    return m_Pending ? six_digits(m_Pending->score) : QString();
}

QString ProfileViewModel::entryName() const
{
    return name_text(m_Name);
}

QStringList ProfileViewModel::recentNames() const
{
    QStringList names;
    for (const profile::PlayerName& name : m_Data.recent_names.names())
    {
        names.push_back(name_text(name));
    }
    return names;
}

QVariantList ProfileViewModel::standing() const
{
    QVariantList lines;
    if (!m_Pending)
    {
        return lines;
    }
    const profile::Leaderboard& board = m_Data.leaderboards.of(m_Pending->mode);
    const std::size_t place = m_Pending->place;
    if (place > 0)
    {
        lines.push_back(line(place, board.at(place - 1).name, board.at(place - 1).score, false));
    }
    lines.push_back(line(place + 1, m_Name, m_Pending->score, true));
    if (place < board.size() && place + 1 < profile::Leaderboard::CAPACITY)
    {
        lines.push_back(line(place + 2, board.at(place).name, board.at(place).score, false));
    }
    return lines;
}

void ProfileViewModel::typeCharacter(const QString& character)
{
    if (!m_Pending || character.size() != 1 || !m_Name.append(character.front().toLatin1()))
    {
        return;
    }
    emit entryChanged();
    emit standingChanged();
}

void ProfileViewModel::eraseCharacter()
{
    if (!m_Pending || m_Name.empty())
    {
        return;
    }
    m_Name.remove_last();
    emit entryChanged();
    emit standingChanged();
}

void ProfileViewModel::chooseRecent(int index)
{
    if (!m_Pending || index < 0 || static_cast<std::size_t>(index) >= m_Data.recent_names.size())
    {
        return;
    }
    m_Name = m_Data.recent_names.at(static_cast<std::size_t>(index));
    emit entryChanged();
    emit standingChanged();
}

bool ProfileViewModel::saveScore()
{
    if (!canSave())
    {
        return false;
    }
    const profile::PlayerName saved = *m_Name.saved();
    const Pending pending = *m_Pending;
    const std::optional<std::size_t> place =
        m_Data.leaderboards.of(pending.mode)
            .insert({.name = saved, .score = pending.score, .unix_seconds = pending.unix_seconds});
    // The place was worked out against this board and nothing has changed it since.
    Q_ASSERT(place.has_value());
    m_Data.recent_names.remember(saved);
    // The leaderboard opens on the mode just played, with the new line marked.
    m_BoardMode = from_sim(pending.mode);
    m_Board.show(pending.mode);
    m_Board.mark_latest(pending.mode, place.value_or(0));
    emit boardModeChanged();
    set_pending(std::nullopt);
    write();
    emit recentNamesChanged();
    emit bestChanged();
    return true;
}

void ProfileViewModel::discardScore()
{
    if (m_Pending)
    {
        set_pending(std::nullopt);
        emit bestChanged();
    }
}

} // namespace snake::ui
