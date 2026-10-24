// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/profile/leaderboard.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace snake::profile
{

std::optional<std::size_t> Leaderboard::rank_for(std::uint32_t score) const noexcept
{
    if (score == 0)
    {
        return std::nullopt;
    }
    // The first entry with a lower score: the new entry goes in front of it and behind every equal score.
    const auto below =
        std::ranges::find_if(entries(), [score](const LeaderboardEntry& entry) { return entry.score < score; });
    const auto rank = static_cast<std::size_t>(below - entries().begin());
    if (rank == CAPACITY)
    {
        return std::nullopt;
    }
    return rank;
}

std::optional<std::size_t> Leaderboard::insert(const LeaderboardEntry& entry) noexcept
{
    const std::optional<std::size_t> rank = rank_for(entry.score);
    if (!rank || !entry.name.is_saved_form())
    {
        return std::nullopt;
    }
    const std::size_t last = std::min(m_Count, CAPACITY - 1);
    for (std::size_t position = last; position > *rank; --position)
    {
        m_Entries[position] = m_Entries[position - 1];
    }
    m_Entries[*rank] = entry;
    m_Count = std::min(m_Count + 1, CAPACITY);
    return rank;
}

std::optional<Leaderboard> Leaderboard::from_entries(std::span<const LeaderboardEntry> entries) noexcept
{
    if (entries.size() > CAPACITY)
    {
        return std::nullopt;
    }
    Leaderboard board;
    for (std::size_t position = 0; position < entries.size(); ++position)
    {
        const LeaderboardEntry& entry = entries[position];
        const bool out_of_order = position > 0 && entry.score > entries[position - 1].score;
        if (entry.score == 0 || out_of_order || !entry.name.is_saved_form())
        {
            return std::nullopt;
        }
        board.m_Entries[position] = entry;
    }
    board.m_Count = entries.size();
    return board;
}

} // namespace snake::profile
