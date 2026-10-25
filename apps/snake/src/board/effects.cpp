// SPDX-License-Identifier: GPL-3.0-or-later
#include "board/effects.hpp"

#include "snake/sim/cell.hpp"
#include "theme/tokens.hpp"

#include <QPoint>
#include <QRgb>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace snake::ui::effects
{

namespace
{

constexpr double PI = 3.14159265358979323846;
constexpr double GRAVITY = 20.0; // cells a second, every second
constexpr std::size_t FOOD_SPECKS = 8;
constexpr std::size_t BONUS_SPECKS = 14;

constexpr QRgb opaque(std::uint32_t rgb)
{
    return 0xFF000000U | rgb;
}

constexpr std::array<QRgb, 3> FOOD_COLOURS = {opaque(tokens::FOOD), opaque(tokens::FOOD_HIGHLIGHT),
                                              opaque(tokens::LEAF)};
constexpr std::array<QRgb, 3> BONUS_COLOURS = {opaque(tokens::MEDAL_GOLD), opaque(tokens::SURFACE),
                                               opaque(tokens::ACCENT)};

} // namespace

std::uint32_t Effects::next_random() noexcept
{
    // xorshift32: a few cheap steps, the same on every platform.
    m_Random ^= m_Random << 13U;
    m_Random ^= m_Random >> 17U;
    m_Random ^= m_Random << 5U;
    return m_Random;
}

double Effects::next_unit() noexcept
{
    return static_cast<double>(next_random() >> 8U) / 16777216.0;
}

void Effects::eaten(sim::Cell cell, std::uint32_t points, bool bonus)
{
    const std::size_t count = bonus ? BONUS_SPECKS : FOOD_SPECKS;
    const auto& colours = bonus ? BONUS_COLOURS : FOOD_COLOURS;
    for (std::size_t i = 0; i < count && m_Particles.size() < MAX_PARTICLES; ++i)
    {
        // Spread round the circle, a little off even, and thrown a little upwards.
        const double angle = (2.0 * PI * (static_cast<double>(i) + (0.6 * next_unit()))) / static_cast<double>(count);
        const double speed = 4.0 + (4.0 * next_unit());
        Particle speck;
        speck.x = static_cast<double>(cell.x) + 0.5;
        speck.y = static_cast<double>(cell.y) + 0.5;
        speck.velocity_x = std::cos(angle) * speed;
        speck.velocity_y = (std::sin(angle) * speed) - 2.0;
        speck.colour = colours[i % colours.size()];
        m_Particles.push_back(speck);
    }
    if (points > 0)
    {
        m_Scores.push_back({.cell = cell, .points = points, .age = 0.0});
    }
}

void Effects::crashed()
{
    if (!m_Reduced)
    {
        m_ShakeAge = 0.0;
    }
}

void Effects::advance(double seconds)
{
    if (!(seconds > 0.0))
    {
        return;
    }
    for (Particle& speck : m_Particles)
    {
        speck.x += speck.velocity_x * seconds;
        speck.y += speck.velocity_y * seconds;
        speck.velocity_y += GRAVITY * seconds;
        speck.age += seconds;
    }
    std::erase_if(m_Particles, [](const Particle& speck) { return speck.age >= PARTICLE_LIFE; });
    for (FloatingScore& score : m_Scores)
    {
        score.age += seconds;
    }
    std::erase_if(m_Scores, [](const FloatingScore& score) { return score.age >= SCORE_LIFE; });
    m_ShakeAge = std::min(m_ShakeAge + seconds, SHAKE_LIFE);
}

void Effects::set_reduced_motion(bool reduced)
{
    m_Reduced = reduced;
    if (reduced)
    {
        m_ShakeAge = SHAKE_LIFE;
    }
}

void Effects::clear()
{
    m_Particles.clear();
    m_Scores.clear();
    m_ShakeAge = SHAKE_LIFE;
}

bool Effects::active() const noexcept
{
    return !m_Particles.empty() || !m_Scores.empty() || m_ShakeAge < SHAKE_LIFE;
}

QPoint Effects::shake() const
{
    if (m_ShakeAge >= SHAKE_LIFE)
    {
        return {};
    }
    // The strength falls away over the life of the shake; the board jitters about 30 times a second.
    const double strength = SHAKE_PIXELS * (1.0 - (m_ShakeAge / SHAKE_LIFE));
    return {static_cast<int>(std::lround(strength * std::sin(m_ShakeAge * 2.0 * PI * 15.0))),
            static_cast<int>(std::lround(strength * std::cos(m_ShakeAge * 2.0 * PI * 11.0)))};
}

} // namespace snake::ui::effects
