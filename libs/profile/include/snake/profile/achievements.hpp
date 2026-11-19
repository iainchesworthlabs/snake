// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/profile/unlocks.hpp"
#include "snake/sim/mode.hpp"
#include "snake/sim/tally.hpp"

#include <cstdint>
#include <optional>

namespace snake::profile
{

/// A feat a finished game can earn. The numbers are written into the save file (and Game Center knows them by name,
/// which the player-facing layers map to), so a new one goes at the end.
enum class Achievement : std::uint8_t
{
    WRAP = 0,         // the snake went off one edge of an Easy board and came out on the other
    BONUS = 1,        // a bonus item was eaten
    COMBO = 2,        // a blueberry was eaten at the top combo
    BEST = 3,         // the score beat a best that was above zero
    SPEED = 4,        // enough was eaten to reach the mode's top speed
    BERRIES_100 = 5,  // the blueberries eaten over every game have reached BERRIES_100_TARGET
    BERRIES_1000 = 6, // the same, for BERRIES_1000_TARGET
    PORTALS = 7,      // the Portals mode is open
    PORTAL = 8,       // the snake went through a portal
    HOT_POTATO = 9,   // the Hot potato mode is open
    SHED = 10,        // the snake shed a tail from a bomb
    ALL_MODES = 11,   // a game has been finished in every mode
};

/// The number of achievements, for checking a stored value.
inline constexpr std::uint8_t ACHIEVEMENT_COUNT = 12;

/// How many blueberries, over every game, earn the two counted achievements. These two are the only numbers in the
/// set that were chosen and not taken from a rule of the game, and they are provisional: tune them from play.
inline constexpr std::uint32_t BERRIES_100_TARGET = 100;
inline constexpr std::uint32_t BERRIES_1000_TARGET = 1000;

/// How far along a counted achievement is.
struct Progress
{
    std::uint32_t current = 0; // never more than `target`
    std::uint32_t target = 0;

    friend constexpr bool operator==(const Progress& lhs, const Progress& rhs) noexcept = default;
};

/// The achievements a player has earned, with the two counts that some of them need. An achievement is for good: it is
/// never taken away, and clearing a leaderboard does not touch it.
///
/// `note_game` and `sync_unlocks` return an Achievements that holds only the achievements that call earned (its two
/// counts are zero). The caller shows them to the player, as it does the Unlocks that record_score() returns.
class Achievements
{
public:
    /// Builds the state from what the save file holds: bit n of `bits` is the achievement numbered n, `blueberries` is
    /// the blueberries eaten in all, and bit n of `modes` is set once a game has been finished in the mode numbered n.
    /// Returns false and changes nothing when a bit is set that is not an achievement or not a mode.
    [[nodiscard]] bool restore(std::uint32_t bits, std::uint32_t blueberries, std::uint8_t modes) noexcept;

    /// The achievement bits to store, in the layout restore() reads.
    [[nodiscard]] std::uint32_t bits() const noexcept
    {
        return m_Bits;
    }

    /// The blueberries eaten in all finished games. It stops at its largest value.
    [[nodiscard]] std::uint32_t blueberries_eaten() const noexcept
    {
        return m_Blueberries;
    }

    /// The modes a game has been finished in, one bit per mode.
    [[nodiscard]] std::uint8_t modes_finished() const noexcept
    {
        return m_Modes;
    }

    [[nodiscard]] bool has(Achievement achievement) const noexcept
    {
        return (m_Bits & bit_of(achievement)) != 0;
    }

    [[nodiscard]] bool none() const noexcept
    {
        return m_Bits == 0;
    }

    /// Notes a finished game and returns the achievements it newly earned, which is empty when it earned nothing new.
    /// `unlocks` is the set after that game's Unlocks::record_score(), and `best_before` the best score of the mode's
    /// leaderboard before the game (0 when it had none). The game's blueberries are added to the running count and
    /// the mode is marked as finished.
    Achievements note_game(sim::Mode mode, std::uint32_t score, const sim::Tally& tally, const Unlocks& unlocks,
                           std::uint32_t best_before) noexcept;

    /// Sets PORTALS and HOT_POTATO from `unlocks` and returns the ones newly set. For a save from before the
    /// achievements, whose unlocks are all it has to go on.
    Achievements sync_unlocks(const Unlocks& unlocks) noexcept;

    /// How far a counted achievement is (BERRIES_100 and BERRIES_1000), or nothing for any other. Once the count has
    /// passed the target, `current` stays at the target.
    [[nodiscard]] std::optional<Progress> progress(Achievement achievement) const noexcept;

    friend bool operator==(const Achievements& lhs, const Achievements& rhs) noexcept = default;

private:
    [[nodiscard]] static constexpr std::uint32_t bit_of(Achievement achievement) noexcept
    {
        return std::uint32_t{1} << static_cast<std::uint8_t>(achievement);
    }

    std::uint32_t m_Bits = 0;
    std::uint32_t m_Blueberries = 0;
    std::uint8_t m_Modes = 0;
};

} // namespace snake::profile
