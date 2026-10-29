// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/scoring.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace snake::sim
{

void Scoring::advance() noexcept
{
    if (m_State.ticks_since_food < LAPSED)
    {
        ++m_State.ticks_since_food;
    }
}

Scoring::FoodResult Scoring::eat_food() noexcept
{
    // A lapsed window starts the combo again at x1, an open one raises it.
    m_State.combo =
        window_open() ? static_cast<std::uint8_t>(std::min<int>(m_State.combo + 1, MAX_COMBO)) : std::uint8_t{1};
    m_State.ticks_since_food = 0;
    ++m_State.foods_eaten;

    const std::uint32_t points = FOOD_POINTS * m_State.combo;
    m_State.score += points;

    ++m_State.normal_since_bonus;
    const bool bonus_due = m_State.normal_since_bonus == BONUS_EVERY;
    if (bonus_due)
    {
        m_State.normal_since_bonus = 0;
    }
    return {.points = points, .bonus_due = bonus_due};
}

std::uint32_t Scoring::eat_bonus() noexcept
{
    if (!window_open())
    {
        m_State.combo = 1;
    }
    m_State.ticks_since_food = 0;
    ++m_State.foods_eaten;
    m_State.score += BONUS_POINTS;
    return BONUS_POINTS;
}

std::uint32_t Scoring::eat_potato() noexcept
{
    if (!window_open())
    {
        m_State.combo = 1;
    }
    m_State.ticks_since_food = 0;
    ++m_State.foods_eaten;
    m_State.score += POTATO_POINTS;
    return POTATO_POINTS;
}

std::array<char, 6> score_digits(std::uint32_t score) noexcept
{
    constexpr std::uint32_t LARGEST = 999'999;
    std::uint32_t rest = std::min(score, LARGEST);
    std::array<char, 6> digits{};
    for (auto place = digits.rbegin(); place != digits.rend(); ++place)
    {
        *place = static_cast<char>('0' + (rest % 10));
        rest /= 10;
    }
    return digits;
}

} // namespace snake::sim
