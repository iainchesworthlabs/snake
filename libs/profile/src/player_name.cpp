// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/profile/player_name.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string_view>

namespace snake::profile
{

namespace
{

// Only called with an allowed character, so a character from 'a' up is a lower-case letter.
constexpr char to_capital(char character) noexcept
{
    return character >= 'a' ? static_cast<char>(character - 'a' + 'A') : character;
}

} // namespace

std::optional<PlayerName> PlayerName::from_text(std::string_view text) noexcept
{
    if (text.size() > MAX_LENGTH)
    {
        return std::nullopt;
    }
    PlayerName name;
    for (const char character : text)
    {
        if (!name.append(character))
        {
            return std::nullopt;
        }
    }
    return name;
}

bool PlayerName::append(char character) noexcept
{
    if (m_Length == MAX_LENGTH || !is_allowed(character))
    {
        return false;
    }
    m_Characters[m_Length] = to_capital(character);
    ++m_Length;
    return true;
}

void PlayerName::remove_last() noexcept
{
    if (m_Length == 0)
    {
        return;
    }
    --m_Length;
    m_Characters[m_Length] = '\0';
}

bool PlayerName::can_save() const noexcept
{
    return std::ranges::any_of(view(), [](char character) { return character != ' '; });
}

std::optional<PlayerName> PlayerName::saved() const noexcept
{
    const std::string_view text = view();
    const std::size_t first = text.find_first_not_of(' ');
    if (first == std::string_view::npos)
    {
        return std::nullopt;
    }
    const std::size_t last = text.find_last_not_of(' ');
    return from_text(text.substr(first, last - first + 1));
}

bool PlayerName::is_saved_form() const noexcept
{
    return can_save() && m_Characters[0] != ' ' && m_Characters[m_Length - 1] != ' ';
}

} // namespace snake::profile
