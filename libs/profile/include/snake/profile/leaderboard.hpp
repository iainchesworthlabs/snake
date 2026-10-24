// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/profile/player_name.hpp"
#include "snake/sim/mode.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace snake::profile
{

/// How many modes there are in version 1, and so how many leaderboards.
inline constexpr std::size_t MODE_COUNT = 2;

[[nodiscard]] constexpr std::size_t mode_index(sim::Mode mode) noexcept
{
    return static_cast<std::size_t>(mode);
}

/// The mode with the given index, or nothing for an index that is not a mode.
[[nodiscard]] constexpr std::optional<sim::Mode> mode_from_index(std::size_t index) noexcept
{
    if (index >= MODE_COUNT)
    {
        return std::nullopt;
    }
    return static_cast<sim::Mode>(index);
}

/// One line of a leaderboard. `unix_seconds` is the time the game ended, in seconds since 1 January 1970 UTC, taken
/// from the platform clock by the caller. It is shown as a date and plays no part in the ranking.
struct LeaderboardEntry
{
    PlayerName name;
    std::uint32_t score = 0;
    std::int64_t unix_seconds = 0;

    friend bool operator==(const LeaderboardEntry& lhs, const LeaderboardEntry& rhs) noexcept = default;
};

/// The top 10 scores of one mode, best first. Of two equal scores the earlier one ranks higher, so a new entry goes
/// below every entry it ties with.
class Leaderboard
{
public:
    static constexpr std::size_t CAPACITY = 10;

    /// The place (0 is the top) a game with `score` would take, or nothing when it would not get on the board: a
    /// score of zero, or a score that does not beat the 10th entry of a full board.
    [[nodiscard]] std::optional<std::size_t> rank_for(std::uint32_t score) const noexcept;

    /// Adds `entry` and returns its place. Returns nothing, changing nothing, when the score does not qualify
    /// (see rank_for()) or the name is not in its saved form. The entry that falls off the end is lost.
    std::optional<std::size_t> insert(const LeaderboardEntry& entry) noexcept;

    /// Builds a board from entries stored earlier, best first. Returns nothing when there are more than CAPACITY of
    /// them, a score is zero or higher than the one before it, or a name is not in its saved form.
    [[nodiscard]] static std::optional<Leaderboard> from_entries(std::span<const LeaderboardEntry> entries) noexcept;

    /// The best score of the mode, or 0 when the board is empty. The gameplay screen shows it as the best score.
    [[nodiscard]] std::uint32_t best_score() const noexcept
    {
        return m_Count == 0 ? 0 : m_Entries[0].score;
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return m_Count;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_Count == 0;
    }

    /// The entry in place `position`, 0 being the top. Needs position < size().
    [[nodiscard]] const LeaderboardEntry& at(std::size_t position) const noexcept
    {
        return m_Entries[position];
    }

    [[nodiscard]] std::span<const LeaderboardEntry> entries() const noexcept
    {
        return {m_Entries.data(), m_Count};
    }

    /// Removes every entry, for the reset in Settings.
    void clear() noexcept
    {
        *this = Leaderboard{};
    }

    friend bool operator==(const Leaderboard& lhs, const Leaderboard& rhs) noexcept
    {
        return std::ranges::equal(lhs.entries(), rhs.entries());
    }

private:
    std::array<LeaderboardEntry, CAPACITY> m_Entries{};
    std::size_t m_Count = 0;
};

/// The leaderboards of all modes. Each mode keeps its own.
class Leaderboards
{
public:
    [[nodiscard]] Leaderboard& of(sim::Mode mode) noexcept
    {
        return m_Boards[mode_index(mode)];
    }

    [[nodiscard]] const Leaderboard& of(sim::Mode mode) const noexcept
    {
        return m_Boards[mode_index(mode)];
    }

    friend bool operator==(const Leaderboards& lhs, const Leaderboards& rhs) noexcept = default;

private:
    std::array<Leaderboard, MODE_COUNT> m_Boards{};
};

} // namespace snake::profile
