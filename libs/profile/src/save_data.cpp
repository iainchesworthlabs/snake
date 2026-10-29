// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/profile/save_data.hpp"

#include "snake/profile/leaderboard.hpp"
#include "snake/profile/player_name.hpp"
#include "snake/profile/recent_names.hpp"
#include "snake/profile/settings.hpp"
#include "snake/profile/unlocks.hpp"
#include "snake/sim/byte_io.hpp"
#include "snake/sim/fnv1a.hpp"
#include "snake/sim/mode.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>

namespace snake::profile
{

namespace
{

constexpr std::array<std::uint8_t, 4> MARKER{'S', 'N', 'K', 'S'};

/// The marker, the version and the payload size.
constexpr std::size_t HEADER_SIZE = 8;
constexpr std::size_t CHECKSUM_SIZE = 8;

constexpr std::uint8_t FLAG_INVERT_AXES = 1U << 0U;
constexpr std::uint8_t FLAG_SOUND = 1U << 1U;
constexpr std::uint8_t FLAG_MUSIC = 1U << 2U;
constexpr std::uint8_t FLAG_REDUCED_MOTION = 1U << 3U;
constexpr std::uint8_t KNOWN_FLAGS = FLAG_INVERT_AXES | FLAG_SOUND | FLAG_MUSIC | FLAG_REDUCED_MOTION;

void put_name(sim::ByteWriter& writer, const PlayerName& name) noexcept
{
    writer.u8(static_cast<std::uint8_t>(name.size()));
    for (const char character : name.view())
    {
        writer.u8(static_cast<std::uint8_t>(character));
    }
}

/// Reads a name. A name with a character that is not a capital letter or a space, or with more than 12 characters,
/// marks the reader bad. A lower-case letter is refused too, so every name has one set of bytes.
PlayerName get_name(sim::ByteReader& reader) noexcept
{
    PlayerName name;
    const std::uint8_t length = reader.u8();
    for (std::uint8_t position = 0; position < length; ++position)
    {
        const auto character = static_cast<char>(reader.u8());
        if (!name.append(character) || character >= 'a')
        {
            reader.fail();
        }
    }
    return name;
}

void put_payload(sim::ByteWriter& writer, const SaveData& data) noexcept
{
    const Settings& settings = data.settings;
    // The control scheme is the low half of the byte and the tilt sensitivity the high half (a file from before it has
    // zero).
    writer.u8(static_cast<std::uint8_t>(static_cast<std::uint8_t>(settings.controls) |
                                        (static_cast<std::uint8_t>(settings.tilt_sensitivity) << 4U)));
    writer.u8(static_cast<std::uint8_t>((settings.invert_axes ? FLAG_INVERT_AXES : 0U) |
                                        (settings.sound ? FLAG_SOUND : 0U) | (settings.music ? FLAG_MUSIC : 0U) |
                                        (settings.reduced_motion ? FLAG_REDUCED_MOTION : 0U)));
    writer.u8(settings.volume);
    writer.u32(data.unlocks.bits());

    writer.u8(static_cast<std::uint8_t>(data.recent_names.size()));
    for (const PlayerName& name : data.recent_names.names())
    {
        put_name(writer, name);
    }

    for (std::size_t index = 0; index < MODE_COUNT; ++index)
    {
        const Leaderboard& board = data.leaderboards.of(static_cast<sim::Mode>(index));
        writer.u8(static_cast<std::uint8_t>(board.size()));
        for (const LeaderboardEntry& entry : board.entries())
        {
            put_name(writer, entry.name);
            writer.u32(entry.score);
            writer.u64(static_cast<std::uint64_t>(entry.unix_seconds));
        }
    }
}

/// Reads the payload of a file of version 1, 2 or 3. Version 2 has the volume byte after the flags, and 3 has the
/// leaderboards of the two modes that came after Easy and Classic. Returns nothing when a value is not allowed or the
/// payload does not end where it should.
std::optional<SaveData> get_payload(std::span<const std::uint8_t> payload, std::uint16_t version) noexcept
{
    sim::ByteReader reader(payload);
    SaveData data;

    const std::uint8_t controls_byte = reader.u8();
    const std::uint8_t controls = controls_byte & 0x0FU;
    const std::uint8_t sensitivity = controls_byte >> 4U;
    const std::uint8_t flags = reader.u8();
    const std::uint8_t volume = version >= 2 ? reader.u8() : VOLUME_MAX;
    const std::uint32_t unlock_bits = reader.u32();
    if (controls >= CONTROL_SCHEME_COUNT || sensitivity >= TILT_SENSITIVITY_COUNT || (flags & ~KNOWN_FLAGS) != 0 ||
        volume > VOLUME_MAX || !data.unlocks.restore(unlock_bits))
    {
        return std::nullopt;
    }
    data.settings.controls = static_cast<ControlScheme>(controls);
    data.settings.tilt_sensitivity = static_cast<TiltSensitivity>(sensitivity);
    data.settings.invert_axes = (flags & FLAG_INVERT_AXES) != 0;
    data.settings.sound = (flags & FLAG_SOUND) != 0;
    data.settings.music = (flags & FLAG_MUSIC) != 0;
    data.settings.reduced_motion = (flags & FLAG_REDUCED_MOTION) != 0;
    data.settings.volume = volume;

    const std::uint8_t recent_count = reader.u8();
    if (recent_count > RecentNames::CAPACITY)
    {
        return std::nullopt;
    }
    std::array<PlayerName, RecentNames::CAPACITY> recent{};
    for (std::uint8_t position = 0; position < recent_count; ++position)
    {
        recent[position] = get_name(reader);
    }
    const std::optional<RecentNames> recent_names = RecentNames::from_names(std::span(recent).first(recent_count));
    if (reader.bad() || !recent_names)
    {
        return std::nullopt;
    }
    data.recent_names = *recent_names;

    // Every version has the boards of Easy and Classic; the others are empty in a file from before they were played.
    const std::size_t boards = version >= 3 ? MODE_COUNT : 2;
    for (std::size_t index = 0; index < boards; ++index)
    {
        const std::uint8_t count = reader.u8();
        if (count > Leaderboard::CAPACITY)
        {
            return std::nullopt;
        }
        std::array<LeaderboardEntry, Leaderboard::CAPACITY> entries{};
        for (std::uint8_t position = 0; position < count; ++position)
        {
            entries[position].name = get_name(reader);
            entries[position].score = reader.u32();
            entries[position].unix_seconds = static_cast<std::int64_t>(reader.u64());
        }
        const std::optional<Leaderboard> board = Leaderboard::from_entries(std::span(entries).first(count));
        if (reader.bad() || !board)
        {
            return std::nullopt;
        }
        data.leaderboards.of(static_cast<sim::Mode>(index)) = *board;
    }

    if (!reader.complete())
    {
        return std::nullopt;
    }
    return data;
}

/// Reads a whole file of version 1, 2 or 3, the marker and the version having been checked.
std::expected<SaveData, SaveError> decode_file(std::span<const std::uint8_t> bytes, std::uint16_t version) noexcept
{
    if (bytes.size() < HEADER_SIZE)
    {
        return std::unexpected(SaveError::TRUNCATED);
    }
    sim::ByteReader header(bytes.subspan(MARKER.size() + 2));
    const std::size_t payload_size = header.u16();
    const std::size_t checksum_at = HEADER_SIZE + payload_size;
    const std::size_t total = checksum_at + CHECKSUM_SIZE;
    if (bytes.size() < total)
    {
        return std::unexpected(SaveError::TRUNCATED);
    }
    if (bytes.size() > total)
    {
        return std::unexpected(SaveError::TRAILING_BYTES);
    }
    sim::ByteReader checksum(bytes.subspan(checksum_at));
    if (checksum.u64() != sim::fnv1a64(bytes.first(checksum_at)))
    {
        return std::unexpected(SaveError::BAD_CHECKSUM);
    }
    const std::optional<SaveData> data = get_payload(bytes.subspan(HEADER_SIZE, payload_size), version);
    if (!data)
    {
        return std::unexpected(SaveError::INVALID_DATA);
    }
    return *data;
}

} // namespace

std::size_t encode_save(const SaveData& data, std::span<std::uint8_t> out) noexcept
{
    // The payload goes in first, because the header holds its size.
    sim::ByteWriter payload(out.size() > HEADER_SIZE ? out.subspan(HEADER_SIZE) : std::span<std::uint8_t>{});
    put_payload(payload, data);
    const std::size_t payload_size = payload.size();
    if (payload_size == 0 || out.size() < HEADER_SIZE + payload_size + CHECKSUM_SIZE)
    {
        return 0;
    }

    sim::ByteWriter header(out);
    for (const std::uint8_t byte : MARKER)
    {
        header.u8(byte);
    }
    header.u16(SAVE_FORMAT_VERSION);
    header.u16(static_cast<std::uint16_t>(payload_size));

    const std::size_t checksum_at = HEADER_SIZE + payload_size;
    sim::ByteWriter checksum(out.subspan(checksum_at));
    checksum.u64(sim::fnv1a64(out.first(checksum_at)));
    return checksum_at + CHECKSUM_SIZE;
}

std::expected<SaveData, SaveError> decode_save(std::span<const std::uint8_t> bytes) noexcept
{
    constexpr std::size_t PREFIX_SIZE = MARKER.size() + 2;
    if (bytes.size() < PREFIX_SIZE)
    {
        return std::unexpected(SaveError::TRUNCATED);
    }
    for (std::size_t position = 0; position < MARKER.size(); ++position)
    {
        if (bytes[position] != MARKER[position])
        {
            return std::unexpected(SaveError::BAD_MARKER);
        }
    }
    sim::ByteReader prefix(bytes.subspan(MARKER.size()));
    const std::uint16_t version = prefix.u16();
    if (version > SAVE_FORMAT_VERSION)
    {
        return std::unexpected(SaveError::NEWER_VERSION);
    }
    // Each version has its own reader, so a file from an older program still opens after an update.
    switch (version)
    {
    case 1:
    case 2:
    case 3:
        return decode_file(bytes, version);
    default:
        return std::unexpected(SaveError::INVALID_DATA);
    }
}

} // namespace snake::profile
