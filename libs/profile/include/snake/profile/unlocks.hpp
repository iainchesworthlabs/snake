// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/sim/mode.hpp"

#include <cstdint>

namespace snake::profile
{

/// A mode that has to be earned. Easy and Classic are always open and are not listed. These are the modes the
/// requirements name that are not in the game yet. The numbers are written into the save file, so a new one goes at
/// the end.
enum class Unlockable : std::uint8_t
{
    PORTALS = 0,
    HOT_POTATO = 1,
};

/// The number of unlockables, for checking a stored value.
inline constexpr std::uint8_t UNLOCKABLE_COUNT = 2;

/// What it takes to open an unlockable: a score of at least `score` in `mode`.
struct UnlockRule
{
    sim::Mode mode = sim::Mode::CLASSIC;
    std::uint32_t score = 0;
};

/// The unlock targets are placeholders to tune in playtesting (requirements, Modes).
[[nodiscard]] constexpr UnlockRule unlock_rule(Unlockable unlockable) noexcept
{
    return unlockable == Unlockable::PORTALS ? UnlockRule{.mode = sim::Mode::CLASSIC, .score = 2000}
                                             : UnlockRule{.mode = sim::Mode::CLASSIC, .score = 3000};
}

/// The unlockables a player has opened. An unlock is for good: clearing a leaderboard does not close it again.
class Unlocks
{
public:
    /// Builds the set from the bits stored in the save file, bit n being the unlockable numbered n. Returns false
    /// and changes nothing when a bit is set that is not an unlockable.
    [[nodiscard]] bool restore(std::uint32_t bits) noexcept;

    /// The bits to store, in the same layout.
    [[nodiscard]] std::uint32_t bits() const noexcept
    {
        return m_Bits;
    }

    [[nodiscard]] bool has(Unlockable unlockable) const noexcept
    {
        return (m_Bits & bit_of(unlockable)) != 0;
    }

    [[nodiscard]] bool none() const noexcept
    {
        return m_Bits == 0;
    }

    /// Notes a finished game. Opens every unlockable whose rule the game meets and returns the ones opened by this
    /// call, which is empty when nothing new opened. The caller shows them to the player.
    Unlocks record_score(sim::Mode mode, std::uint32_t score) noexcept;

    friend bool operator==(const Unlocks& lhs, const Unlocks& rhs) noexcept = default;

private:
    [[nodiscard]] static constexpr std::uint32_t bit_of(Unlockable unlockable) noexcept
    {
        return std::uint32_t{1} << static_cast<std::uint8_t>(unlockable);
    }

    std::uint32_t m_Bits = 0;
};

} // namespace snake::profile
