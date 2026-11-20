// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/profile/achievements.hpp"
#include "snake/profile/leaderboard.hpp"
#include "snake/profile/player_name.hpp"
#include "snake/profile/recent_names.hpp"
#include "snake/profile/settings.hpp"
#include "snake/profile/unlocks.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace snake::profile
{

/// Everything a player keeps between runs, apart from recorded games: the settings, the unlocks, the achievements, the
/// recent names and the leaderboards. It is small (at most MAX_ENCODED_SAVE_SIZE bytes), so the platform service writes
/// it as one file and replaces the whole file on every change. The last game and the best game of each mode are replays
/// (snake/sim/replay.hpp), which are much larger, change at other times and have a format of their own, so the
/// platform stores them as separate files.
struct SaveData
{
    Settings settings;
    Unlocks unlocks;
    Achievements achievements;
    RecentNames recent_names;
    Leaderboards leaderboards;

    friend bool operator==(const SaveData& lhs, const SaveData& rhs) noexcept = default;
};

/// The version of the format encode_save() writes.
///
/// The first six bytes of a save file (the marker "SNKS" and the version) are the same in every version, so any
/// version of the program can tell what it was given. Everything after them belongs to that version. A new version
/// adds a reader for its own layout and keeps the readers of the older ones, which fill in the new fields with their
/// defaults, so an update never loses what the player had. The writer only ever writes the newest version.
inline constexpr std::uint16_t SAVE_FORMAT_VERSION = 4;

/// The most bytes encode_save() writes. A full save: the marker, version, size and checksum (8 + 8 bytes), the
/// settings (3), unlocks (4) and achievements (9), three recent names, and ten entries for each mode.
inline constexpr std::size_t MAX_ENCODED_SAVE_SIZE =
    8 + 8 + 3 + 4 + 9 + (1 + (RecentNames::CAPACITY * (1 + PlayerName::MAX_LENGTH))) +
    (MODE_COUNT * (1 + (Leaderboard::CAPACITY * ((1 + PlayerName::MAX_LENGTH) + 4 + 8))));

/// Writes `data` as bytes in the current version and returns how many. Returns 0, having written something
/// useless, when `out` is too small (MAX_ENCODED_SAVE_SIZE is always enough).
///
/// Layout (version 4), whole numbers least significant byte first:
///   "SNKS", version u16, payload size u16,
///   payload: control scheme u8 (low half; the tilt sensitivity is the high half),
///            flags u8 (bit 0 inverted axes, 1 sound, 2 music, 3 reduced motion), volume u8 (0 to VOLUME_MAX),
///            unlock bits u32, achievement bits u32, blueberries eaten u32, modes finished u8 (bit n for the mode
///            numbered n), recent names (count u8, then each name),
///            for each mode in order (Easy, Classic, Portals, Hot potato): entry count u8, then each entry: name, score
///            u32, end time i64 (seconds since 1 January 1970 UTC),
///   checksum u64 (FNV-1a of every byte before it).
/// A name is its length u8 followed by that many capital letters or spaces.
/// Version 3 is the same without the three achievement fields. Version 2 has the leaderboards of Easy and Classic only,
/// and version 1 is that without the volume byte. A file of version 1, 2 or 3 is read with nothing earned and both
/// counts at zero, except that the achievements for opening Portals and Hot potato are set from its unlock bits; a file
/// of version 1 or 2 is read with the other leaderboards empty, and a version 1 file with the volume at its loudest.
[[nodiscard]] std::size_t encode_save(const SaveData& data, std::span<std::uint8_t> out) noexcept;

/// Why bytes could not be read as a save.
enum class SaveError : std::uint8_t
{
    TRUNCATED,      // the bytes end before the file does
    BAD_MARKER,     // not a save file
    NEWER_VERSION,  // written by a newer program: the caller must not overwrite it with an older program's save
    TRAILING_BYTES, // bytes left over after the checksum
    BAD_CHECKSUM,   // damaged
    INVALID_DATA,   // the checksum matches but a value is not allowed
};

/// Reads bytes written by encode_save() in any version up to SAVE_FORMAT_VERSION. Nothing it is given makes it read
/// past the end or fail in any way other than an error value.
[[nodiscard]] std::expected<SaveData, SaveError> decode_save(std::span<const std::uint8_t> bytes) noexcept;

} // namespace snake::profile
