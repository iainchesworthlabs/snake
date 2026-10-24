// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/sim/direction.hpp"

#include <cstdint>
#include <optional>

namespace snake::input
{

/// A place on the touch screen. The unit is the one the platform chooses, which should be a device-independent pixel
/// so that a swipe is the same physical length on every screen. y grows downwards, as touch screens report it.
struct TouchPoint
{
    float x = 0.0F;
    float y = 0.0F;
};

struct SwipeConfig
{
    /// How far a finger travels along its larger axis to make a swipe, in the unit of TouchPoint. 24 device-independent
    /// pixels is about four millimetres.
    float min_distance = 24.0F;
};

/// Recognises a swipe anywhere on the screen and gives its direction.
///
/// The direction is given as soon as the finger has travelled far enough, without waiting for it to lift, so the snake
/// turns while the player is still moving. The finger can carry on in a new direction without lifting: each time a
/// direction is given, the point it was measured from moves to where the finger is, and the next swipe is measured
/// from there. A touch that never goes far enough, a tap, gives nothing.
///
/// Only one finger is followed, the first to touch. Others are ignored until it lifts. The platform calls cancel()
/// when the system takes the touch away (a call, a notification, losing focus), because no release follows then.
class SwipeRecogniser
{
public:
    explicit SwipeRecogniser(SwipeConfig config = {}) noexcept : m_Config(config) {}

    /// A finger touches down. It is followed unless another finger already is. A second press of the finger that is
    /// followed starts again from the new point.
    void press(std::int32_t pointer, TouchPoint at) noexcept;

    /// The finger moves. Returns the direction of a swipe that this movement completes.
    std::optional<sim::Direction> move(std::int32_t pointer, TouchPoint at) noexcept;

    /// The finger lifts. Releasing a finger that is not followed changes nothing.
    void release(std::int32_t pointer) noexcept;

    /// Stops following any finger.
    void cancel() noexcept
    {
        m_Tracking = false;
    }

    [[nodiscard]] bool tracking() const noexcept
    {
        return m_Tracking;
    }

private:
    SwipeConfig m_Config;
    bool m_Tracking = false;
    std::int32_t m_Pointer = 0;
    TouchPoint m_Anchor;
};

} // namespace snake::input
