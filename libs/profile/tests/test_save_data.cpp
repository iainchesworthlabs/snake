// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/profile/leaderboard.hpp"
#include "snake/profile/player_name.hpp"
#include "snake/profile/recent_names.hpp"
#include "snake/profile/save_data.hpp"
#include "snake/profile/settings.hpp"
#include "snake/profile/unlocks.hpp"
#include "snake/sim/fnv1a.hpp"
#include "snake/sim/mode.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string_view>
#include <vector>

using namespace snake::profile;
using snake::sim::Mode;

namespace
{

using Bytes = std::vector<std::uint8_t>;

PlayerName name_of(std::string_view text)
{
    const auto name = PlayerName::from_text(text);
    REQUIRE(name.has_value());
    return *name;
}

void add_entry(Leaderboard& board, std::string_view name, std::uint32_t score, std::int64_t unix_seconds)
{
    REQUIRE(board.insert(LeaderboardEntry{.name = name_of(name), .score = score, .unix_seconds = unix_seconds})
                .has_value());
}

/// The save the pinned bytes below were worked out for.
SaveData sample()
{
    SaveData data;
    data.settings = Settings{.controls = ControlScheme::SWIPE,
                             .invert_axes = true,
                             .sound = true,
                             .music = false,
                             .volume = 6,
                             .reduced_motion = true};
    REQUIRE(data.unlocks.restore(0b01));
    data.recent_names.remember(name_of("MARY JANE"));
    data.recent_names.remember(name_of("IAIN"));
    add_entry(data.leaderboards.of(Mode::EASY), "IAIN", 1230, 1'761'264'000);
    add_entry(data.leaderboards.of(Mode::EASY), "MARY JANE", 450, 1'761'000'000);
    add_entry(data.leaderboards.of(Mode::CLASSIC), "IAIN", 2450, 1'761'300'000);
    return data;
}

// Both byte arrays were laid out and checksummed by a separate Python script that shares no code with the library.
// These are version 1 files, as the first builds wrote them; the program still reads them.
constexpr std::array<std::uint8_t, 25> FIRST_RUN_BYTES_V1{
    0x53, 0x4E, 0x4B, 0x53, 0x01, 0x00, 0x09, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x68, 0x08, 0x36, 0x2D, 0x82, 0x2B, 0x8D, 0x75,
};

constexpr std::array<std::uint8_t, 96> SAMPLE_BYTES_V1{
    0x53, 0x4E, 0x4B, 0x53, 0x01, 0x00, 0x50, 0x00, 0x01, 0x0B, 0x01, 0x00, 0x00, 0x00, 0x02, 0x04,
    0x49, 0x41, 0x49, 0x4E, 0x09, 0x4D, 0x41, 0x52, 0x59, 0x20, 0x4A, 0x41, 0x4E, 0x45, 0x02, 0x04,
    0x49, 0x41, 0x49, 0x4E, 0xCE, 0x04, 0x00, 0x00, 0x80, 0xC1, 0xFA, 0x68, 0x00, 0x00, 0x00, 0x00,
    0x09, 0x4D, 0x41, 0x52, 0x59, 0x20, 0x4A, 0x41, 0x4E, 0x45, 0xC2, 0x01, 0x00, 0x00, 0x40, 0xBA,
    0xF6, 0x68, 0x00, 0x00, 0x00, 0x00, 0x01, 0x04, 0x49, 0x41, 0x49, 0x4E, 0x92, 0x09, 0x00, 0x00,
    0x20, 0x4E, 0xFB, 0x68, 0x00, 0x00, 0x00, 0x00, 0xEC, 0xA6, 0x9B, 0xF8, 0xCD, 0xB6, 0xE2, 0xC7,
};

// The same two saves in version 2, which has the volume byte after the flags (the sample has a volume of 6). The
// checksums were worked out again by the same script.
constexpr std::array<std::uint8_t, 26> FIRST_RUN_BYTES{
    0x53, 0x4E, 0x4B, 0x53, 0x02, 0x00, 0x0A, 0x00, 0x00, 0x06, 0x0A, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xFA, 0x36, 0xA6, 0x19, 0xF9, 0xEA, 0xF2, 0xB1,
};

constexpr std::array<std::uint8_t, 97> SAMPLE_BYTES{
    0x53, 0x4E, 0x4B, 0x53, 0x02, 0x00, 0x51, 0x00, 0x01, 0x0B, 0x06, 0x01, 0x00, 0x00, 0x00, 0x02, 0x04,
    0x49, 0x41, 0x49, 0x4E, 0x09, 0x4D, 0x41, 0x52, 0x59, 0x20, 0x4A, 0x41, 0x4E, 0x45, 0x02, 0x04, 0x49,
    0x41, 0x49, 0x4E, 0xCE, 0x04, 0x00, 0x00, 0x80, 0xC1, 0xFA, 0x68, 0x00, 0x00, 0x00, 0x00, 0x09, 0x4D,
    0x41, 0x52, 0x59, 0x20, 0x4A, 0x41, 0x4E, 0x45, 0xC2, 0x01, 0x00, 0x00, 0x40, 0xBA, 0xF6, 0x68, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x04, 0x49, 0x41, 0x49, 0x4E, 0x92, 0x09, 0x00, 0x00, 0x20, 0x4E, 0xFB, 0x68,
    0x00, 0x00, 0x00, 0x00, 0x56, 0x42, 0x34, 0x3D, 0xEC, 0x5B, 0xC8, 0x04,
};

Bytes encoded(const SaveData& data)
{
    Bytes bytes(MAX_ENCODED_SAVE_SIZE);
    const std::size_t size = encode_save(data, bytes);
    REQUIRE(size > 0);
    bytes.resize(size);
    return bytes;
}

Bytes to_bytes(std::span<const std::uint8_t> bytes)
{
    return Bytes(bytes.begin(), bytes.end());
}

void append_u64(Bytes& bytes, std::uint64_t value)
{
    for (unsigned shift = 0; shift < 64; shift += 8)
    {
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

/// A file around `payload`: the marker, version 2, the size and a correct checksum. For building files that are
/// well formed except for one value.
Bytes file_of(const Bytes& payload)
{
    Bytes bytes{'S',
                'N',
                'K',
                'S',
                2,
                0,
                static_cast<std::uint8_t>(payload.size()),
                static_cast<std::uint8_t>(payload.size() >> 8U)};
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    append_u64(bytes, snake::sim::fnv1a64(bytes));
    return bytes;
}

/// `bytes` with the checksum worked out again, for changing one value in a file.
Bytes with_new_checksum(Bytes bytes)
{
    bytes.resize(bytes.size() - 8);
    append_u64(bytes, snake::sim::fnv1a64(bytes));
    return bytes;
}

Bytes joined(std::initializer_list<Bytes> parts)
{
    Bytes all;
    for (const Bytes& part : parts)
    {
        all.insert(all.end(), part.begin(), part.end());
    }
    return all;
}

/// A name as stored: its length, then the characters.
Bytes stored_name(std::string_view text)
{
    Bytes bytes{static_cast<std::uint8_t>(text.size())};
    for (const char character : text)
    {
        bytes.push_back(static_cast<std::uint8_t>(character));
    }
    return bytes;
}

/// A board entry as stored: name, score, then the time.
Bytes stored_entry(std::string_view name, std::uint32_t score)
{
    Bytes bytes = stored_name(name);
    for (unsigned shift = 0; shift < 32; shift += 8)
    {
        bytes.push_back(static_cast<std::uint8_t>(score >> shift));
    }
    append_u64(bytes, 0);
    return bytes;
}

Bytes settings_and_unlocks()
{
    return Bytes{0, 0x03, VOLUME_MAX, 0, 0, 0, 0};
}

Bytes no_recent_names()
{
    return Bytes{0};
}

Bytes empty_board()
{
    return Bytes{0};
}

/// A payload with the given recent names and Easy board, and an empty Classic board.
Bytes payload_with(const Bytes& recent, const Bytes& easy)
{
    return joined({settings_and_unlocks(), recent, easy, empty_board()});
}

void check_invalid(const Bytes& file)
{
    const auto result = decode_save(file);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == SaveError::INVALID_DATA);
}

} // namespace

TEST_CASE("A first-run save has the bytes worked out by hand", "[save][format]")
{
    CHECK(encoded(SaveData{}) == to_bytes(FIRST_RUN_BYTES));
}

TEST_CASE("A save with settings, unlocks, names and both boards has the bytes worked out by hand", "[save][format]")
{
    CHECK(encoded(sample()) == to_bytes(SAMPLE_BYTES));
}

TEST_CASE("The bytes of a save read back as the save", "[save][format]")
{
    const auto first_run = decode_save(FIRST_RUN_BYTES);
    REQUIRE(first_run.has_value());
    CHECK(*first_run == SaveData{});

    const auto result = decode_save(SAMPLE_BYTES);
    REQUIRE(result.has_value());
    CHECK(*result == sample());
    CHECK(result->settings.controls == ControlScheme::SWIPE);
    CHECK(result->leaderboards.of(Mode::EASY).at(1).name.view() == "MARY JANE");
    CHECK(result->leaderboards.of(Mode::CLASSIC).best_score() == 2450);
    CHECK(result->recent_names.at(0).view() == "IAIN");
}

TEST_CASE("A version 1 file still reads, with the volume at its loudest", "[save][version]")
{
    const auto first_run = decode_save(FIRST_RUN_BYTES_V1);
    REQUIRE(first_run.has_value());
    CHECK(*first_run == SaveData{});
    CHECK(first_run->settings.volume == VOLUME_MAX);

    SaveData expected = sample();
    expected.settings.volume = VOLUME_MAX;
    const auto result = decode_save(SAMPLE_BYTES_V1);
    REQUIRE(result.has_value());
    CHECK(*result == expected);
}

TEST_CASE("A version 1 file with a value that is not allowed is refused as before", "[save][version][invalid]")
{
    Bytes bytes = to_bytes(SAMPLE_BYTES_V1);
    bytes[8] = 4; // there is no fifth control scheme
    check_invalid(with_new_checksum(bytes));
    bytes = to_bytes(SAMPLE_BYTES_V1);
    bytes.insert(bytes.end() - 8, 0); // a byte too many in the payload (the size is now wrong too)
    CHECK_FALSE(decode_save(bytes).has_value());
}

TEST_CASE("Every volume survives a round trip, and a volume above the loudest is refused", "[save][roundtrip]")
{
    for (std::uint8_t volume = 0; volume <= VOLUME_MAX; ++volume)
    {
        SaveData data;
        data.settings.volume = volume;
        const auto result = decode_save(encoded(data));
        REQUIRE(result.has_value());
        CHECK(result->settings.volume == volume);
    }

    Bytes bytes = to_bytes(SAMPLE_BYTES);
    bytes[10] = VOLUME_MAX + 1;
    check_invalid(with_new_checksum(bytes));
}

TEST_CASE("Every combination of settings survives a round trip", "[save][roundtrip]")
{
    for (const ControlScheme controls :
         {ControlScheme::TILT, ControlScheme::SWIPE, ControlScheme::DPAD, ControlScheme::JOYSTICK})
    {
        for (const TiltSensitivity sensitivity :
             {TiltSensitivity::NORMAL, TiltSensitivity::QUICK, TiltSensitivity::GENTLE})
        {
            for (unsigned flags = 0; flags < 16; ++flags)
            {
                SaveData data;
                data.settings = Settings{.controls = controls,
                                         .tilt_sensitivity = sensitivity,
                                         .invert_axes = (flags & 1U) != 0,
                                         .sound = (flags & 2U) != 0,
                                         .music = (flags & 4U) != 0,
                                         .reduced_motion = (flags & 8U) != 0};
                const auto result = decode_save(encoded(data));
                REQUIRE(result.has_value());
                CHECK(result->settings == data.settings);
            }
        }
    }
}

TEST_CASE("Every unlock combination survives a round trip", "[save][roundtrip]")
{
    for (std::uint32_t bits = 0; bits < (1U << UNLOCKABLE_COUNT); ++bits)
    {
        SaveData data;
        REQUIRE(data.unlocks.restore(bits));
        const auto result = decode_save(encoded(data));
        REQUIRE(result.has_value());
        CHECK(result->unlocks.bits() == bits);
    }
}

TEST_CASE("A full save fits in MAX_ENCODED_SAVE_SIZE and reads back", "[save][roundtrip]")
{
    SaveData data;
    data.recent_names.remember(name_of("AAAAAAAAAAAA"));
    data.recent_names.remember(name_of("BBBBBBBBBBBB"));
    data.recent_names.remember(name_of("CCCCCCCCCCCC"));
    for (const Mode mode : {Mode::EASY, Mode::CLASSIC})
    {
        for (std::uint32_t place = 0; place < Leaderboard::CAPACITY; ++place)
        {
            add_entry(data.leaderboards.of(mode), "ZZZZZZZZZZZZ", 1000 - place, -5'000'000'000LL);
        }
    }
    const Bytes bytes = encoded(data);
    CHECK(bytes.size() == MAX_ENCODED_SAVE_SIZE);
    const auto result = decode_save(bytes);
    REQUIRE(result.has_value());
    CHECK(*result == data);
}

TEST_CASE("A save that is too small to hold the file is not written", "[save][encode]")
{
    const SaveData data = sample();
    const std::size_t size = SAMPLE_BYTES.size();
    Bytes out(size);
    CHECK(encode_save(data, out) == size);
    for (const std::size_t room :
         {std::size_t{0}, std::size_t{7}, std::size_t{8}, std::size_t{9}, size - 9, size - 8, size - 1})
    {
        Bytes small(room);
        CHECK(encode_save(data, small) == 0);
    }
}

TEST_CASE("A file that ends early is truncated, wherever it ends", "[save][decode]")
{
    for (std::size_t length = 0; length < SAMPLE_BYTES.size(); ++length)
    {
        const auto result = decode_save(std::span(SAMPLE_BYTES).first(length));
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == SaveError::TRUNCATED);
    }
}

TEST_CASE("Bytes after the checksum are refused", "[save][decode]")
{
    Bytes bytes = to_bytes(SAMPLE_BYTES);
    bytes.push_back(0);
    const auto result = decode_save(bytes);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == SaveError::TRAILING_BYTES);
}

TEST_CASE("Bytes that are not a save file are refused", "[save][decode]")
{
    Bytes bytes = to_bytes(SAMPLE_BYTES);
    bytes[3] = 'X';
    const auto result = decode_save(bytes);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == SaveError::BAD_MARKER);

    const auto replay = decode_save(Bytes{'S', 'N', 'K', 'R', 1, 0, 0, 0});
    REQUIRE_FALSE(replay.has_value());
    CHECK(replay.error() == SaveError::BAD_MARKER);
}

TEST_CASE("A file from a newer program is reported as newer, not as damaged", "[save][decode][version]")
{
    for (const std::uint8_t version : std::array<std::uint8_t, 3>{3, 4, 255})
    {
        Bytes bytes{'S', 'N', 'K', 'S', version, 0};
        const auto result = decode_save(bytes);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == SaveError::NEWER_VERSION);
    }
    const auto high = decode_save(Bytes{'S', 'N', 'K', 'S', 0, 1, 0xDE, 0xAD, 0xBE});
    REQUIRE_FALSE(high.has_value());
    CHECK(high.error() == SaveError::NEWER_VERSION);
}

TEST_CASE("Version 0 never existed", "[save][decode][version]")
{
    Bytes bytes = to_bytes(SAMPLE_BYTES);
    bytes[4] = 0;
    const auto result = decode_save(bytes);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == SaveError::INVALID_DATA);
}

TEST_CASE("Changing any byte after the version is caught by the checksum or the size", "[save][decode]")
{
    for (std::size_t position = 6; position < SAMPLE_BYTES.size(); ++position)
    {
        Bytes bytes = to_bytes(SAMPLE_BYTES);
        bytes[position] ^= 0x01;
        const auto result = decode_save(bytes);
        REQUIRE_FALSE(result.has_value());
        if (position < 8)
        {
            // The size is wrong, so the file looks too short or too long.
            CHECK((result.error() == SaveError::TRUNCATED || result.error() == SaveError::TRAILING_BYTES));
        }
        else
        {
            CHECK(result.error() == SaveError::BAD_CHECKSUM);
        }
    }
}

TEST_CASE("A control scheme that does not exist is refused", "[save][decode][invalid]")
{
    Bytes bytes = to_bytes(SAMPLE_BYTES);
    bytes[8] = 4; // 0 to 3 are the schemes (the joystick is 3)
    check_invalid(with_new_checksum(bytes));
}

TEST_CASE("A tilt sensitivity that does not exist is refused, and one that does is read", "[save][decode][invalid]")
{
    Bytes bytes = to_bytes(SAMPLE_BYTES);
    const std::uint8_t scheme = bytes[8];
    bytes[8] = static_cast<std::uint8_t>(scheme | 0x30U); // 0 to 2 are the sensitivities
    check_invalid(with_new_checksum(bytes));
    bytes[8] = static_cast<std::uint8_t>(scheme | 0x20U);
    const auto result = decode_save(with_new_checksum(bytes));
    REQUIRE(result.has_value());
    CHECK(result->settings.tilt_sensitivity == TiltSensitivity::GENTLE);
    CHECK(result->settings.controls == ControlScheme::SWIPE);
}

TEST_CASE("A flag that does not exist is refused", "[save][decode][invalid]")
{
    Bytes bytes = to_bytes(SAMPLE_BYTES);
    bytes[9] |= 0x10;
    check_invalid(with_new_checksum(bytes));
}

TEST_CASE("An unlock that does not exist is refused", "[save][decode][invalid]")
{
    Bytes bytes = to_bytes(SAMPLE_BYTES);
    bytes[11] = 0x04;
    check_invalid(with_new_checksum(bytes));
}

TEST_CASE("Recent names that cannot be right are refused", "[save][decode][invalid]")
{
    SECTION("more than three")
    {
        const Bytes recent = joined({Bytes{4}, stored_name("A"), stored_name("B"), stored_name("C"), stored_name("D")});
        check_invalid(file_of(payload_with(recent, empty_board())));
    }
    SECTION("a name twice")
    {
        const Bytes recent = joined({Bytes{2}, stored_name("ANN"), stored_name("ANN")});
        check_invalid(file_of(payload_with(recent, empty_board())));
    }
    SECTION("a name with a space at the end")
    {
        check_invalid(file_of(payload_with(joined({Bytes{1}, stored_name("ANN ")}), empty_board())));
    }
    SECTION("a name of spaces")
    {
        check_invalid(file_of(payload_with(joined({Bytes{1}, stored_name("   ")}), empty_board())));
    }
    SECTION("a name with no characters")
    {
        check_invalid(file_of(payload_with(joined({Bytes{1}, stored_name("")}), empty_board())));
    }
    SECTION("a name cut short")
    {
        check_invalid(file_of(joined({settings_and_unlocks(), Bytes{1, 5, 'A', 'B'}})));
    }
}

TEST_CASE("A name with a character that is not a capital letter or a space is refused", "[save][decode][invalid]")
{
    SECTION("a digit")
    {
        check_invalid(file_of(payload_with(joined({Bytes{1}, stored_name("R2D2")}), empty_board())));
    }
    SECTION("a lower-case letter")
    {
        check_invalid(file_of(payload_with(joined({Bytes{1}, stored_name("Ann")}), empty_board())));
    }
    SECTION("a byte above 127")
    {
        check_invalid(file_of(payload_with(joined({Bytes{1, 1, 0xC3}}), empty_board())));
    }
    SECTION("13 characters")
    {
        check_invalid(file_of(payload_with(joined({Bytes{1}, stored_name("ABCDEFGHIJKLM")}), empty_board())));
    }
}

TEST_CASE("A leaderboard that cannot be right is refused", "[save][decode][invalid]")
{
    SECTION("more than ten entries")
    {
        check_invalid(file_of(payload_with(no_recent_names(), Bytes{11})));
    }
    SECTION("entries out of order")
    {
        const Bytes easy = joined({Bytes{2}, stored_entry("ANN", 100), stored_entry("BOB", 200)});
        check_invalid(file_of(payload_with(no_recent_names(), easy)));
    }
    SECTION("a score of zero")
    {
        const Bytes easy = joined({Bytes{1}, stored_entry("ANN", 0)});
        check_invalid(file_of(payload_with(no_recent_names(), easy)));
    }
    SECTION("a name with a space at the start")
    {
        const Bytes easy = joined({Bytes{1}, stored_entry(" ANN", 10)});
        check_invalid(file_of(payload_with(no_recent_names(), easy)));
    }
    SECTION("an entry cut short")
    {
        check_invalid(file_of(joined({settings_and_unlocks(), no_recent_names(), Bytes{1, 3, 'A', 'N', 'N', 10}})));
    }
    SECTION("a Classic board that is missing")
    {
        check_invalid(file_of(joined({settings_and_unlocks(), no_recent_names(), empty_board()})));
    }
}

TEST_CASE("A payload with bytes left over is refused", "[save][decode][invalid]")
{
    Bytes payload = payload_with(no_recent_names(), empty_board());
    payload.push_back(0);
    check_invalid(file_of(payload));
}

TEST_CASE("Any one value changed behind a correct checksum is refused or reads back the same", "[save][decode][robust]")
{
    // The payload runs from byte 8 to byte 88. Every file that is accepted must be one that encode_save() would write.
    for (std::size_t position = 8; position < SAMPLE_BYTES.size() - 8; ++position)
    {
        for (const std::uint8_t value : std::array<std::uint8_t, 6>{0x00, 0x01, 0x20, 0x7F, 0x80, 0xFF})
        {
            Bytes bytes = to_bytes(SAMPLE_BYTES);
            bytes[position] = value;
            bytes = with_new_checksum(bytes);
            const auto result = decode_save(bytes);
            if (result.has_value())
            {
                CHECK(encoded(*result) == bytes);
            }
        }
    }
}

TEST_CASE("Equal times and names on a board are allowed, and the order is kept", "[save][roundtrip]")
{
    SaveData data;
    add_entry(data.leaderboards.of(Mode::EASY), "ANN", 300, 7);
    add_entry(data.leaderboards.of(Mode::EASY), "BOB", 300, 7);
    const auto result = decode_save(encoded(data));
    REQUIRE(result.has_value());
    CHECK(result->leaderboards.of(Mode::EASY).at(0).name.view() == "ANN");
    CHECK(result->leaderboards.of(Mode::EASY).at(1).name.view() == "BOB");
}

TEST_CASE("Saves are equal when every part is", "[save]")
{
    const SaveData base;
    CHECK(base == SaveData{});

    SaveData changed;
    changed.settings.sound = false;
    CHECK_FALSE(base == changed);

    changed = base;
    REQUIRE(changed.unlocks.restore(1));
    CHECK_FALSE(base == changed);

    changed = base;
    changed.recent_names.remember(name_of("ANN"));
    CHECK_FALSE(base == changed);

    changed = base;
    add_entry(changed.leaderboards.of(Mode::EASY), "ANN", 10, 0);
    CHECK_FALSE(base == changed);
}

TEST_CASE("The save format version is 2", "[save][version]")
{
    CHECK(SAVE_FORMAT_VERSION == 2);
    CHECK(MAX_ENCODED_SAVE_SIZE == 8 + 8 + 3 + 4 + (1 + (3 * 13)) + (2 * (1 + (10 * 25))));
}
