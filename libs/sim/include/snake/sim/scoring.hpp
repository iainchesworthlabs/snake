// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace snake::sim
{

/// The score, the combo and the food count of one game.
///
/// Food is worth 10 points times the combo. Eating again within 20 ticks of the last food (normal or bonus) raises
/// the combo by one, up to x5. Letting the window pass drops it back to x1. Bonus food is worth 50 points flat: it
/// restarts the window but does not raise the combo. Every fifth normal food is followed by a bonus item.
///
/// The class counts ticks only to time the window. It knows nothing about the board.
class Scoring
{
public:
    static constexpr std::uint32_t FOOD_POINTS = 10;
    static constexpr std::uint32_t BONUS_POINTS = 50;
    static constexpr std::uint32_t POTATO_POINTS = 100;
    static constexpr std::uint8_t MAX_COMBO = 5;
    /// A food eaten this many ticks after the last one still continues the combo.
    static constexpr std::uint8_t COMBO_WINDOW_TICKS = 20;
    /// A bonus item follows every this many normal foods.
    static constexpr std::uint8_t BONUS_EVERY = 5;

    /// The value of State::ticks_since_food once the window has passed.
    static constexpr std::uint8_t LAPSED = COMBO_WINDOW_TICKS + 1;

    /// Everything that is needed to carry on a game, for saving and for setting up tests.
    struct State
    {
        std::uint32_t score = 0;
        std::uint16_t foods_eaten = 0;          // normal and bonus food, for the speed curve
        std::uint8_t normal_since_bonus = 0;    // normal foods eaten since the last bonus item appeared, 0 to 4
        std::uint8_t combo = 1;                 // 1 to MAX_COMBO
        std::uint8_t ticks_since_food = LAPSED; // saturates at LAPSED, which is also the value before the first food

        friend constexpr bool operator==(const State& lhs, const State& rhs) noexcept = default;
    };

    Scoring() noexcept = default;

    /// `state` must be valid().
    explicit Scoring(const State& state) noexcept : m_State(state) {}

    [[nodiscard]] static constexpr bool valid(const State& state) noexcept
    {
        return state.combo >= 1 && state.combo <= MAX_COMBO && state.normal_since_bonus < BONUS_EVERY &&
               state.ticks_since_food <= LAPSED;
    }

    /// One tick has started. Call it once per tick, before any eating.
    void advance() noexcept;

    /// What eating a normal food did.
    struct FoodResult
    {
        std::uint32_t points = 0;
        bool bonus_due = false; // this was every fifth food: a bonus item should appear
    };

    [[nodiscard]] FoodResult eat_food() noexcept;

    /// Eating the bonus item. Returns the points.
    std::uint32_t eat_bonus() noexcept;

    /// Eating the bomb food of Hot potato: POTATO_POINTS flat, and the window is restarted without the combo being
    /// raised, as the bonus item does it. Returns the points.
    std::uint32_t eat_potato() noexcept;

    [[nodiscard]] std::uint32_t score() const noexcept
    {
        return m_State.score;
    }

    [[nodiscard]] std::size_t foods_eaten() const noexcept
    {
        return m_State.foods_eaten;
    }

    /// The multiplier now, as the display shows it: 1 once the window has passed.
    [[nodiscard]] std::uint8_t combo() const noexcept
    {
        return window_open() ? m_State.combo : std::uint8_t{1};
    }

    [[nodiscard]] const State& state() const noexcept
    {
        return m_State;
    }

private:
    [[nodiscard]] bool window_open() const noexcept
    {
        return m_State.ticks_since_food <= COMBO_WINDOW_TICKS;
    }

    State m_State;
};

/// Digits shown for a score: six, with leading zeros. A score above 999999 shows as 999999.
[[nodiscard]] std::array<char, 6> score_digits(std::uint32_t score) noexcept;

} // namespace snake::sim
