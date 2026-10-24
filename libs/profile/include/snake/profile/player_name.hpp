// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace snake::profile
{

/// A name for the leaderboard: up to 12 characters, each a letter from A to Z or a space, kept in capitals.
///
/// The name entry screen builds one with append() and remove_last(). A name can be saved once it has a letter in it
/// (can_save()); saved() gives the form that goes on the leaderboard, with the spaces at both ends taken off.
class PlayerName
{
public:
    static constexpr std::size_t MAX_LENGTH = 12;

    /// True for the characters a name may hold: the letters A to Z, in either case, and the space.
    [[nodiscard]] static constexpr bool is_allowed(char character) noexcept
    {
        return character == ' ' || (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
    }

    /// Reads a name from text. Returns nothing when the text is longer than MAX_LENGTH or holds a character that is
    /// not allowed. Lower-case letters become capitals.
    [[nodiscard]] static std::optional<PlayerName> from_text(std::string_view text) noexcept;

    /// Adds `character` at the end, as a capital. Returns false, changing nothing, when the name is full or the
    /// character is not allowed.
    bool append(char character) noexcept;

    /// Takes the last character off. Does nothing when the name is empty.
    void remove_last() noexcept;

    void clear() noexcept
    {
        *this = PlayerName{};
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return m_Length;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_Length == 0;
    }

    [[nodiscard]] std::string_view view() const noexcept
    {
        return {m_Characters.data(), m_Length};
    }

    /// True when the name has at least one letter. A name of spaces cannot be saved.
    [[nodiscard]] bool can_save() const noexcept;

    /// The name as it goes on the leaderboard: the spaces at the start and the end removed. Returns nothing when
    /// the name cannot be saved.
    [[nodiscard]] std::optional<PlayerName> saved() const noexcept;

    /// True for a name that saved() would give back unchanged: it has a letter and no space at either end.
    [[nodiscard]] bool is_saved_form() const noexcept;

    friend bool operator==(const PlayerName& lhs, const PlayerName& rhs) noexcept
    {
        return lhs.view() == rhs.view();
    }

private:
    std::array<char, MAX_LENGTH> m_Characters{};
    std::uint8_t m_Length = 0;
};

} // namespace snake::profile
