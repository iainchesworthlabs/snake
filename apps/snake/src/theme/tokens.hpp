// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string_view>

namespace snake::ui::tokens
{

/// The Toybox Pixel colours as 0xRRGGBB. docs/design/tokens.json is the source; a test compares each value with it,
/// so a change to the file fails until this one follows.
inline constexpr std::uint32_t BACKGROUND = 0xFFC93C;
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
inline constexpr std::uint32_t MEDAL_BRONZE = 0xF0A868;

/// The pause dimming: INK at this opacity, as in the `overlay` token (rgba(43,29,82,0.8)).
inline constexpr float OVERLAY_ALPHA = 0.8F;

/// The font family of all text. Weights 400, 500, 600 and 700 are bundled.
inline constexpr std::string_view FONT_FAMILY = "Pixelify Sans";

} // namespace snake::ui::tokens
