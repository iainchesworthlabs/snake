// SPDX-License-Identifier: GPL-3.0-or-later
#include "theme/theme.hpp"

#include "theme/tokens.hpp"

#include <QFontDatabase>
#include <QStringList>
#include <array>
#include <cstdint>

namespace snake::ui
{

namespace
{

constexpr std::array<const char*, 4> FONT_FILES = {
    ":/snake/fonts/pixelify-sans-latin-400-normal.woff2",
    ":/snake/fonts/pixelify-sans-latin-500-normal.woff2",
    ":/snake/fonts/pixelify-sans-latin-600-normal.woff2",
    ":/snake/fonts/pixelify-sans-latin-700-normal.woff2",
};

QColor colour_of(std::uint32_t rgb)
{
    return QColor::fromRgb(rgb);
}

} // namespace

bool load_fonts()
{
    bool all_loaded = true;
    for (const char* file : FONT_FILES)
    {
        all_loaded = QFontDatabase::addApplicationFont(QString::fromLatin1(file)) >= 0 && all_loaded;
    }
    return all_loaded;
}

QFont application_font()
{
    QFont font;
    font.setFamilies({QString::fromLatin1(tokens::FONT_FAMILY.data(), tokens::FONT_FAMILY.size()),
                      QStringLiteral("Courier New"), QStringLiteral("monospace")});
    return font;
}

Theme::Theme(QObject* parent) : QObject(parent) {}

QColor Theme::background() const
{
    return colour_of(tokens::BACKGROUND);
}

QColor Theme::ink() const
{
    return colour_of(tokens::INK);
}

QColor Theme::shadow() const
{
    return colour_of(tokens::SHADOW);
}

QColor Theme::surface() const
{
    return colour_of(tokens::SURFACE);
}

QColor Theme::accent() const
{
    return colour_of(tokens::ACCENT);
}

QColor Theme::overlay() const
{
    QColor colour = colour_of(tokens::INK);
    colour.setAlphaF(tokens::OVERLAY_ALPHA);
    return colour;
}

QString Theme::fontFamily() const
{
    return QString::fromLatin1(tokens::FONT_FAMILY.data(), tokens::FONT_FAMILY.size());
}

} // namespace snake::ui
