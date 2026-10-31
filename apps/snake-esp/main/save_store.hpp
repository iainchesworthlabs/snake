// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/profile/save_data.hpp"

namespace snake::esp
{

/// The saved profile (the settings, the unlocks and the leaderboards) in the flash's non-volatile storage: one blob, in
/// the format that libs/profile writes for the phones, under a namespace of its own.
///
/// It never erases the storage. On a board that is running other firmware, Snake shares the partition with it (the
/// network settings of that firmware are in it), so when the storage cannot be opened Snake runs without saving, and
/// when a value of its own cannot be read it starts again from the defaults.
class SaveStore
{
public:
    /// Opens the storage. When it cannot be opened, load returns the defaults and store does nothing.
    SaveStore();

    /// What was saved, or the defaults when nothing was saved, or what was saved is damaged.
    [[nodiscard]] snake::profile::SaveData load();

    /// Saves the profile, unless what is there was written by a newer version of the game: that is left alone.
    void store(const snake::profile::SaveData& data);

private:
    bool m_Ready = false;
    bool m_Writable = true;
};

} // namespace snake::esp
