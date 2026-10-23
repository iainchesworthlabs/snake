// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace snake
{

/// How the main window is created. The old code asked for 1024 by 768 with 4x multisampling. The
/// title is not here: it is user-visible text and lives in the translated user interface.
struct WindowSettings
{
    int width;
    int height;
    int multisample_count;
};

inline constexpr WindowSettings MAIN_WINDOW{1024, 768, 4};

} // namespace snake
