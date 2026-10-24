// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/byte_io.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

using namespace snake::sim;

TEST_CASE("Numbers are written least significant byte first", "[byte_io]")
{
    std::array<std::uint8_t, 15> bytes{};
    ByteWriter writer(bytes);
    writer.u8(0x01);
    writer.u16(0x0302);
    writer.u32(0x07060504);
    writer.u64(0x0F0E0D0C0B0A0908ULL);
    CHECK(writer.size() == 15);
    const std::array<std::uint8_t, 15> expected{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                                0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    CHECK(bytes == expected);
}

TEST_CASE("Numbers read back as they were written", "[byte_io]")
{
    std::array<std::uint8_t, 15> bytes{};
    ByteWriter writer(bytes);
    writer.u8(0xAB);
    writer.u16(0xCDEF);
    writer.u32(0x12345678);
    writer.u64(0x1122334455667788ULL);
    ByteReader reader(bytes);
    CHECK(reader.u8() == 0xAB);
    CHECK(reader.u16() == 0xCDEF);
    CHECK(reader.u32() == 0x12345678);
    CHECK(reader.u64() == 0x1122334455667788ULL);
    CHECK(reader.complete());
    CHECK_FALSE(reader.bad());
}

TEST_CASE("A writer that runs out of room never writes past the end and reports size 0", "[byte_io]")
{
    std::array<std::uint8_t, 3> bytes{};
    ByteWriter writer(bytes);
    writer.u32(0xFFFFFFFF);
    CHECK(writer.size() == 0);
    CHECK(bytes[2] == 0xFF);

    std::array<std::uint8_t, 4> exact{};
    ByteWriter fits(exact);
    fits.u32(1);
    CHECK(fits.size() == 4);
}

TEST_CASE("Varints use 7 bits a byte and the top bit says another follows", "[byte_io][varint]")
{
    std::array<std::uint8_t, 16> bytes{};
    ByteWriter writer(bytes);
    writer.varint(0);
    writer.varint(127);
    writer.varint(128);
    writer.varint(300);
    writer.varint(0xFFFFFFFF);
    CHECK(writer.size() == 1 + 1 + 2 + 2 + 5);
    const std::array<std::uint8_t, 11> expected{0x00, 0x7F, 0x80, 0x01, 0xAC, 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
    for (std::size_t position = 0; position < expected.size(); ++position)
    {
        CHECK(bytes[position] == expected[position]);
    }

    ByteReader reader(std::span<const std::uint8_t>(bytes).first(11));
    CHECK(reader.varint() == 0);
    CHECK(reader.varint() == 127);
    CHECK(reader.varint() == 128);
    CHECK(reader.varint() == 300);
    CHECK(reader.varint() == 0xFFFFFFFF);
    CHECK(reader.complete());
}

TEST_CASE("Reading past the end gives zeros and marks the reader bad", "[byte_io]")
{
    const std::array<std::uint8_t, 2> bytes{0x34, 0x12};
    ByteReader reader(bytes);
    CHECK(reader.u32() == 0x1234);
    CHECK(reader.bad());
    CHECK_FALSE(reader.complete());
    CHECK(reader.u8() == 0);
}

TEST_CASE("A reader is not complete while bytes are left over", "[byte_io]")
{
    const std::array<std::uint8_t, 2> bytes{1, 2};
    ByteReader reader(bytes);
    CHECK(reader.u8() == 1);
    CHECK_FALSE(reader.bad());
    CHECK_FALSE(reader.complete());
    CHECK(reader.u8() == 2);
    CHECK(reader.complete());
}

TEST_CASE("fail marks a reader bad", "[byte_io]")
{
    const std::array<std::uint8_t, 1> bytes{1};
    ByteReader reader(bytes);
    CHECK(reader.u8() == 1);
    reader.fail();
    CHECK(reader.bad());
    CHECK_FALSE(reader.complete());
}

TEST_CASE("A varint that is cut short, or too big for 32 bits, is bad", "[byte_io][varint]")
{
    const std::array<std::uint8_t, 2> cut{0x80, 0x80};
    ByteReader short_reader(cut);
    static_cast<void>(short_reader.varint());
    CHECK(short_reader.bad());

    const std::array<std::uint8_t, 5> big{0xFF, 0xFF, 0xFF, 0xFF, 0x1F};
    ByteReader big_reader(big);
    static_cast<void>(big_reader.varint());
    CHECK(big_reader.bad());

    const std::array<std::uint8_t, 6> six{0x80, 0x80, 0x80, 0x80, 0x80, 0x00};
    ByteReader six_reader(six);
    CHECK(six_reader.varint() == 0);
    CHECK(six_reader.bad());
}
