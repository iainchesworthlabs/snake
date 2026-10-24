// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/profile/recent_names.hpp"

#include "snake/profile/player_name.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>

namespace snake::profile
{

void RecentNames::remember(const PlayerName& name) noexcept
{
    const std::optional<PlayerName> saved = name.saved();
    if (!saved)
    {
        return;
    }
    // Where the name is now, or the end of the list when it is new. The names in front of it move back one place,
    // which also drops the oldest name when the list is full and the name is new.
    const auto known = std::ranges::find(names(), *saved);
    const std::size_t from = static_cast<std::size_t>(known - names().begin());
    const std::size_t last = (from == m_Count && m_Count == CAPACITY) ? CAPACITY - 1 : from;
    for (std::size_t position = last; position > 0; --position)
    {
        m_Names[position] = m_Names[position - 1];
    }
    m_Names[0] = *saved;
    m_Count = std::max(m_Count, last + 1);
}

std::optional<RecentNames> RecentNames::from_names(std::span<const PlayerName> names) noexcept
{
    if (names.size() > CAPACITY)
    {
        return std::nullopt;
    }
    RecentNames recent;
    for (std::size_t position = 0; position < names.size(); ++position)
    {
        const bool repeated = std::ranges::find(names.first(position), names[position]) != names.first(position).end();
        if (!names[position].is_saved_form() || repeated)
        {
            return std::nullopt;
        }
        recent.m_Names[position] = names[position];
    }
    recent.m_Count = names.size();
    return recent;
}

} // namespace snake::profile
