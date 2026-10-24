// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/profile/player_name.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <span>

namespace snake::profile
{

/// The names used most recently, newest first, offered on the name entry screen as one-tap shortcuts so that young
/// players never retype theirs. A name appears once: using it again moves it to the front.
class RecentNames
{
public:
    static constexpr std::size_t CAPACITY = 3;

    /// Notes that `name` was just used, in its saved form. A name that cannot be saved is ignored. When the list is
    /// full the oldest name falls off.
    void remember(const PlayerName& name) noexcept;

    /// Builds the list from names stored earlier, newest first. Returns nothing when there are more than CAPACITY of
    /// them, when one is not in its saved form, or when a name appears twice.
    [[nodiscard]] static std::optional<RecentNames> from_names(std::span<const PlayerName> names) noexcept;

    [[nodiscard]] std::size_t size() const noexcept
    {
        return m_Count;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_Count == 0;
    }

    /// The name `position` places from the front, 0 being the newest. Needs position < size().
    [[nodiscard]] const PlayerName& at(std::size_t position) const noexcept
    {
        return m_Names[position];
    }

    [[nodiscard]] std::span<const PlayerName> names() const noexcept
    {
        return {m_Names.data(), m_Count};
    }

    void clear() noexcept
    {
        *this = RecentNames{};
    }

    friend bool operator==(const RecentNames& lhs, const RecentNames& rhs) noexcept
    {
        return std::ranges::equal(lhs.names(), rhs.names());
    }

private:
    std::array<PlayerName, CAPACITY> m_Names{};
    std::size_t m_Count = 0;
};

} // namespace snake::profile
