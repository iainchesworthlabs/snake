// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace snake::sim
{

/// Writes numbers into a buffer in the byte form used by snapshots, replays and the save file: whole numbers
/// least significant byte first, so the bytes are the same on every platform. It never writes past the end. It
/// counts what it was asked to write, and size() says whether it all fitted.
class ByteWriter
{
public:
    explicit ByteWriter(std::span<std::uint8_t> out) noexcept : m_Out(out) {}

    void u8(std::uint8_t value) noexcept
    {
        if (m_Size < m_Out.size())
        {
            m_Out[m_Size] = value;
        }
        ++m_Size;
    }

    void u16(std::uint16_t value) noexcept
    {
        u8(static_cast<std::uint8_t>(value));
        u8(static_cast<std::uint8_t>(value >> 8U));
    }

    void u32(std::uint32_t value) noexcept
    {
        u16(static_cast<std::uint16_t>(value));
        u16(static_cast<std::uint16_t>(value >> 16U));
    }

    void u64(std::uint64_t value) noexcept
    {
        u32(static_cast<std::uint32_t>(value));
        u32(static_cast<std::uint32_t>(value >> 32U));
    }

    /// A number in groups of 7 bits, low group first. A byte has its top bit set when another follows.
    void varint(std::uint32_t value) noexcept
    {
        while (value >= 0x80U)
        {
            u8(static_cast<std::uint8_t>((value & 0x7FU) | 0x80U));
            value >>= 7U;
        }
        u8(static_cast<std::uint8_t>(value));
    }

    /// The bytes written, or 0 when they did not fit.
    [[nodiscard]] std::size_t size() const noexcept
    {
        return m_Size <= m_Out.size() ? m_Size : 0;
    }

private:
    std::span<std::uint8_t> m_Out;
    std::size_t m_Size = 0;
};

/// Reads numbers written by ByteWriter. Reading past the end gives zeros and marks the reader bad, so the caller
/// can read a whole record and check once.
class ByteReader
{
public:
    /// The most bytes a varint holds: 5 groups of 7 bits cover 32 bits.
    static constexpr std::size_t MAX_VARINT_BYTES = 5;

    explicit ByteReader(std::span<const std::uint8_t> bytes) noexcept : m_Bytes(bytes) {}

    std::uint8_t u8() noexcept
    {
        if (m_Position >= m_Bytes.size())
        {
            m_Bad = true;
            return 0;
        }
        return m_Bytes[m_Position++];
    }

    std::uint16_t u16() noexcept
    {
        const std::uint16_t low = u8();
        const std::uint16_t high = u8();
        return static_cast<std::uint16_t>(low | (high << 8U));
    }

    std::uint32_t u32() noexcept
    {
        const std::uint32_t low = u16();
        const std::uint32_t high = u16();
        return low | (high << 16U);
    }

    std::uint64_t u64() noexcept
    {
        const std::uint64_t low = u32();
        const std::uint64_t high = u32();
        return low | (high << 32U);
    }

    /// Reads a number written by ByteWriter::varint(). A number that is cut short or that needs more than 32 bits
    /// marks the reader bad.
    std::uint32_t varint() noexcept
    {
        std::uint64_t value = 0;
        for (std::size_t group = 0; group < MAX_VARINT_BYTES; ++group)
        {
            const std::uint8_t byte = u8();
            value |= static_cast<std::uint64_t>(byte & 0x7FU) << (7 * group);
            if ((byte & 0x80U) == 0)
            {
                if (value > 0xFFFFFFFFULL)
                {
                    m_Bad = true;
                }
                return static_cast<std::uint32_t>(value);
            }
        }
        // A sixth group is one too many for a 32-bit number.
        m_Bad = true;
        return 0;
    }

    /// Marks the reader bad, for a value that was read fine but is not allowed.
    void fail() noexcept
    {
        m_Bad = true;
    }

    /// True when a read ran past the end or fail() was called.
    [[nodiscard]] bool bad() const noexcept
    {
        return m_Bad;
    }

    /// True when every byte read so far existed and none is left over.
    [[nodiscard]] bool complete() const noexcept
    {
        return !m_Bad && m_Position == m_Bytes.size();
    }

private:
    std::span<const std::uint8_t> m_Bytes;
    std::size_t m_Position = 0;
    bool m_Bad = false;
};

} // namespace snake::sim
