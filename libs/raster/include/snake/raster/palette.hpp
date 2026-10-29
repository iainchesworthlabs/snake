// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

namespace snake::raster
{

/// The Toybox Pixel colours the board is drawn in, as 0xRRGGBB. They are the board's share of docs/design/tokens.json,
/// and a test of the application compares each with the colours its own scene draws in, so the two cannot drift apart.
namespace palette
{
inline constexpr std::uint32_t INK = 0x2B1D52;
inline constexpr std::uint32_t SHADOW = 0x1A1035;
inline constexpr std::uint32_t SURFACE = 0xFFF1D6;
inline constexpr std::uint32_t ACCENT = 0xFF6B4A;
inline constexpr std::uint32_t BOARD_LIGHT = 0x1F6E7A;
inline constexpr std::uint32_t BOARD_DARK = 0x1A6170;
inline constexpr std::uint32_t FOOD = 0x4C6BFF;
inline constexpr std::uint32_t FOOD_HIGHLIGHT = 0xDDE3FF;
inline constexpr std::uint32_t LEAF = 0x5BD16A;
inline constexpr std::uint32_t TONGUE = 0xFF2E63;
inline constexpr std::uint32_t EYE = 0xFFFFFF;
inline constexpr std::uint32_t MEDAL_GOLD = 0xFFC93C;
inline constexpr std::uint32_t MEDAL_SILVER = 0xD7DCEB;
} // namespace palette

/// A colour as a 16-bit pixel: 5 bits of red, 6 of green and 5 of blue, the format of the ESP32-P4's panel.
[[nodiscard]] constexpr std::uint16_t rgb565(std::uint32_t rgb) noexcept
{
    return static_cast<std::uint16_t>(((rgb >> 8U) & 0xF800U) | ((rgb >> 5U) & 0x07E0U) | ((rgb >> 3U) & 0x001FU));
}

} // namespace snake::raster
