// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/sound_sink.hpp"

namespace snake::ui
{

namespace
{

SoundSink* g_default_sink = nullptr;

} // namespace

SoundSink* default_sound_sink() noexcept
{
    return g_default_sink;
}

void set_default_sound_sink(SoundSink* sink) noexcept
{
    g_default_sink = sink;
}

} // namespace snake::ui
