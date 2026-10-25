// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace snake::ui
{

/// True when the system asks for less motion (the reduced-motion accessibility setting). The first run starts with the
/// game's own reduced-motion setting on when it is. False when there is no GUI application to ask.
[[nodiscard]] bool system_prefers_reduced_motion();

} // namespace snake::ui
