// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/haptics.hpp"

namespace snake::ui::device
{

namespace
{

Haptics* g_default_haptics = nullptr;

} // namespace

Haptics* default_haptics() noexcept
{
    return g_default_haptics;
}

void set_default_haptics(Haptics* haptics) noexcept
{
    g_default_haptics = haptics;
}

} // namespace snake::ui::device
