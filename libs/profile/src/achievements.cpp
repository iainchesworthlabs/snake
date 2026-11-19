// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/profile/achievements.hpp"

#include "snake/profile/unlocks.hpp"
#include "snake/sim/mode.hpp"
#include "snake/sim/scoring.hpp"
#include "snake/sim/tally.hpp"

#include <cstdint>
#include <limits>
#include <optional>

namespace snake::profile
{

namespace
{

constexpr std::uint32_t bit_of(Achievement achievement) noexcept
{
    return std::uint32_t{1} << static_cast<std::uint8_t>(achievement);
}

constexpr std::uint8_t ALL_MODES_MASK = static_cast<std::uint8_t>((1U << sim::MODE_COUNT) - 1U);

/// The bit of `condition` for `achievement`, so a rule reads as one line.
constexpr std::uint32_t when(bool condition, Achievement achievement) noexcept
{
    return condition ? bit_of(achievement) : 0U;
}

} // namespace

bool Achievements::restore(std::uint32_t bits, std::uint32_t blueberries, std::uint8_t modes) noexcept
{
    if ((bits >> ACHIEVEMENT_COUNT) != 0 || (modes & ~ALL_MODES_MASK) != 0)
    {
        return false;
    }
    m_Bits = bits;
    m_Blueberries = blueberries;
    m_Modes = modes;
    return true;
}

Achievements Achievements::note_game(sim::Mode mode, std::uint32_t score, const sim::Tally& tally,
                                     const Unlocks& unlocks, std::uint32_t best_before) noexcept
{
    const std::uint32_t room = std::numeric_limits<std::uint32_t>::max() - m_Blueberries;
    m_Blueberries += tally.blueberries() < room ? tally.blueberries() : room;
    m_Modes |= static_cast<std::uint8_t>(1U << static_cast<std::uint8_t>(mode));

    const std::uint32_t met =
        when(tally.wrapped(), Achievement::WRAP) | when(tally.bonus_items() > 0, Achievement::BONUS) |
        when(tally.top_combo() >= sim::Scoring::MAX_COMBO, Achievement::COMBO) |
        when(best_before > 0 && score > best_before, Achievement::BEST) |
        when(sim::tick_interval_ms(mode, tally.foods_eaten()) == sim::speed_curve_of(mode).floor_ms,
             Achievement::SPEED) |
        when(m_Blueberries >= BERRIES_100_TARGET, Achievement::BERRIES_100) |
        when(m_Blueberries >= BERRIES_1000_TARGET, Achievement::BERRIES_1000) |
        when(unlocks.has(Unlockable::PORTALS), Achievement::PORTALS) |
        when(tally.portals_used() > 0, Achievement::PORTAL) |
        when(unlocks.has(Unlockable::HOT_POTATO), Achievement::HOT_POTATO) |
        when(tally.sheds() > 0, Achievement::SHED) | when(m_Modes == ALL_MODES_MASK, Achievement::ALL_MODES);

    Achievements earned;
    earned.m_Bits = met & ~m_Bits;
    m_Bits |= met;
    return earned;
}

Achievements Achievements::sync_unlocks(const Unlocks& unlocks) noexcept
{
    const std::uint32_t met = when(unlocks.has(Unlockable::PORTALS), Achievement::PORTALS) |
                              when(unlocks.has(Unlockable::HOT_POTATO), Achievement::HOT_POTATO);
    Achievements earned;
    earned.m_Bits = met & ~m_Bits;
    m_Bits |= met;
    return earned;
}

std::optional<Progress> Achievements::progress(Achievement achievement) const noexcept
{
    std::uint32_t target = 0;
    switch (achievement)
    {
    case Achievement::BERRIES_100:
        target = BERRIES_100_TARGET;
        break;
    case Achievement::BERRIES_1000:
        target = BERRIES_1000_TARGET;
        break;
    default:
        return std::nullopt;
    }
    return Progress{.current = m_Blueberries < target ? m_Blueberries : target, .target = target};
}

} // namespace snake::profile
