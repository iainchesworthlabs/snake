// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/sim/cell.hpp"

#include <QPoint>
#include <QRgb>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace snake::ui::effects
{

/// A speck thrown out when the snake eats. Positions are in cells from the top left of the board, so they do not
/// depend on how big the board is drawn.
struct Particle
{
    double x = 0.0;
    double y = 0.0;
    double velocity_x = 0.0; // cells a second
    double velocity_y = 0.0;
    double age = 0.0;
    QRgb colour = 0;
};

/// The points of a food, rising from the cell where it was eaten.
struct FloatingScore
{
    sim::Cell cell;
    std::uint32_t points = 0;
    double age = 0.0;
};

/// The short-lived show of the game: the burst of specks and the rising points when the snake eats, and the shake of
/// the board when it crashes. It holds no game rules and takes nothing from the simulation but the cells and points it
/// is told, and its own random numbers never touch the game's generator, so it can differ from one run to the next
/// without changing a game. It is driven by time: advance() moves everything on.
///
/// Reduced motion turns the shake off. The specks and the points stay, because they are not shake or flashing.
class Effects
{
public:
    /// The most specks alive at once, so a run of foods cannot fill the screen (and the small displays can take it).
    static constexpr std::size_t MAX_PARTICLES = 64;
    /// How long a speck lives, a floating score lives and the shake lasts, in seconds.
    static constexpr double PARTICLE_LIFE = 0.5;
    static constexpr double SCORE_LIFE = 0.8;
    static constexpr double SHAKE_LIFE = 0.35;
    /// How far a floating score rises, in cells.
    static constexpr double SCORE_RISE = 1.5;
    /// The shake at its strongest, in art pixels.
    static constexpr double SHAKE_PIXELS = 3.0;

    /// The snake ate at `cell` and scored `points`. A bonus item gives a bigger burst in gold.
    void eaten(sim::Cell cell, std::uint32_t points, bool bonus);

    /// The snake crashed: the board shakes, unless motion is reduced.
    void crashed();

    /// Moves everything on by `seconds`. A time that is not above zero does nothing.
    void advance(double seconds);

    void set_reduced_motion(bool reduced);
    [[nodiscard]] bool reduced_motion() const noexcept
    {
        return m_Reduced;
    }

    /// Drops everything, for a new game.
    void clear();

    /// True while anything is still moving.
    [[nodiscard]] bool active() const noexcept;

    [[nodiscard]] std::span<const Particle> particles() const noexcept
    {
        return m_Particles;
    }

    [[nodiscard]] std::span<const FloatingScore> scores() const noexcept
    {
        return m_Scores;
    }

    /// How far the board is shifted now, in art pixels: nothing when it is not shaking.
    [[nodiscard]] QPoint shake() const;

    /// How far through its life a floating score is, from 0 to 1.
    [[nodiscard]] static double score_progress(const FloatingScore& score) noexcept
    {
        return score.age / SCORE_LIFE;
    }

private:
    [[nodiscard]] std::uint32_t next_random() noexcept;
    /// A number from 0 up to but not including 1.
    [[nodiscard]] double next_unit() noexcept;

    std::vector<Particle> m_Particles;
    std::vector<FloatingScore> m_Scores;
    double m_ShakeAge = SHAKE_LIFE; // the shake is over when the age reaches SHAKE_LIFE
    bool m_Reduced = false;
    std::uint32_t m_Random = 0x2545F491U;
};

} // namespace snake::ui::effects
