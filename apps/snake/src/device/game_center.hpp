// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <functional>
#include <string_view>
#include <utility>

namespace snake::ui::device
{

/// Apple's Game Center, for the leaderboards and achievements that the players of one Apple account share. Only an
/// iPhone or iPad has it; every other device uses a GameCenter that does nothing, and the game's own leaderboards and
/// achievements, kept on the device, are the same with or without it.
class GameCenter
{
public:
    GameCenter() = default;
    GameCenter(const GameCenter&) = delete;
    GameCenter& operator=(const GameCenter&) = delete;
    GameCenter(GameCenter&&) = delete;
    GameCenter& operator=(GameCenter&&) = delete;
    virtual ~GameCenter() = default;

    /// Whether the player is signed in to Game Center now, so that a score can be sent and the leaderboards shown.
    [[nodiscard]] virtual bool available() const = 0;

    /// Sends a score to the leaderboard with the identifier from App Store Connect. Nothing happens when the player is
    /// not signed in. A score that cannot be sent (no connection) is not tried again.
    virtual void submit_score(std::string_view leaderboard_id, std::int64_t score) = 0;

    /// Shows Game Center's own screen for the leaderboard over the game. Nothing happens when the player is not signed
    /// in.
    virtual void show_leaderboard(std::string_view leaderboard_id) = 0;

    /// Reports how far the player is towards the achievement with the identifier from App Store Connect: 100 when it is
    /// earned, and below that for one that is counted. Nothing happens when the player is not signed in. A report that
    /// cannot be sent (no connection) is not tried again; the one that goes out the next time Game Center becomes
    /// available covers it. Reporting the same thing twice is harmless.
    virtual void report_achievement(std::string_view achievement_id, double percent) = 0;

    /// Shows Game Center's own screen for the achievements over the game. Nothing happens when the player is not
    /// signed in.
    virtual void show_achievements() = 0;

    /// Calls `callback` (on the thread of the user interface) when available() may have changed: the player signed in
    /// or out. An empty function stops the calls.
    void set_on_change(std::function<void()> callback)
    {
        m_OnChange = std::move(callback);
    }

protected:
    void notify_change() const
    {
        if (m_OnChange)
        {
            m_OnChange();
        }
    }

private:
    std::function<void()> m_OnChange;
};

/// A GameCenter that does nothing, for every device but an iPhone or iPad.
class NoGameCenter final : public GameCenter
{
public:
    [[nodiscard]] bool available() const override
    {
        return false;
    }
    void submit_score(std::string_view /*leaderboard_id*/, std::int64_t /*score*/) override {}
    void show_leaderboard(std::string_view /*leaderboard_id*/) override {}
    void report_achievement(std::string_view /*achievement_id*/, double /*percent*/) override {}
    void show_achievements() override {}
};

/// The Game Center the game uses: the one the program set, or none, in which case there is no Game Center. Set it
/// before the user interface is created and clear it after. It is not thread safe.
[[nodiscard]] GameCenter* default_game_center() noexcept;
void set_default_game_center(GameCenter* game_center) noexcept;

} // namespace snake::ui::device
