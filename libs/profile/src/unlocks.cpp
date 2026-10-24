// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/profile/unlocks.hpp"

#include "snake/sim/mode.hpp"

#include <cstdint>

namespace snake::profile
{

bool Unlocks::restore(std::uint32_t bits) noexcept
{
    if ((bits >> UNLOCKABLE_COUNT) != 0)
    {
        return false;
    }
    m_Bits = bits;
    return true;
}

Unlocks Unlocks::record_score(sim::Mode mode, std::uint32_t score) noexcept
{
    Unlocks opened;
    for (std::uint8_t number = 0; number < UNLOCKABLE_COUNT; ++number)
    {
        const auto unlockable = static_cast<Unlockable>(number);
        const UnlockRule rule = unlock_rule(unlockable);
        if (!has(unlockable) && mode == rule.mode && score >= rule.score)
        {
            opened.m_Bits |= bit_of(unlockable);
        }
    }
    m_Bits |= opened.m_Bits;
    return opened;
}

} // namespace snake::profile
