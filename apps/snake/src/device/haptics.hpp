// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace snake::ui::device
{

/// The buzz of a phone, for what the player feels in the hand. A computer has none, and the game plays on without it.
class Haptics
{
public:
    Haptics() = default;
    Haptics(const Haptics&) = delete;
    Haptics& operator=(const Haptics&) = delete;
    Haptics(Haptics&&) = delete;
    Haptics& operator=(Haptics&&) = delete;
    virtual ~Haptics() = default;

    /// A short light tap: the snake ate.
    virtual void tap() = 0;
    /// A longer, harder knock: the snake crashed.
    virtual void thud() = 0;
};

/// A Haptics that does nothing, for a device with no motor.
class NoHaptics final : public Haptics
{
public:
    void tap() override {}
    void thud() override {}
};

/// The haptics the game uses: the one the program set, or none, in which case nothing buzzes. Set it before the user
/// interface is created and clear it after. It is not thread safe.
[[nodiscard]] Haptics* default_haptics() noexcept;
void set_default_haptics(Haptics* haptics) noexcept;

} // namespace snake::ui::device
