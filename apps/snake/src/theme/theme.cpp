// SPDX-License-Identifier: GPL-3.0-or-later
#include "theme/theme.hpp"

#include "theme/tokens.hpp"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QStringList>
#include <array>
#include <cstdint>

namespace snake::ui
{

namespace
{

constexpr std::array<const char*, 10> FONT_FILES = {
    ":/snake/fonts/pixelify-sans-latin-400-normal.woff2",
    ":/snake/fonts/pixelify-sans-latin-500-normal.woff2",
    ":/snake/fonts/pixelify-sans-latin-600-normal.woff2",
    ":/snake/fonts/pixelify-sans-latin-700-normal.woff2",
    ":/snake/fonts/noto-sans-jp-400.woff2",
    ":/snake/fonts/noto-sans-jp-700.woff2",
    ":/snake/fonts/noto-sans-sc-400.woff2",
    ":/snake/fonts/noto-sans-sc-700.woff2",
    ":/snake/fonts/noto-sans-hebrew-400.woff2",
    ":/snake/fonts/noto-sans-hebrew-700.woff2",
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

QFont application_font(const QLocale& locale)
{
    // Japanese and Chinese share Han characters that are drawn differently, so the language chooses which comes first.
    const bool chinese = locale.language() == QLocale::Chinese;
    const QString japanese = QStringLiteral("Noto Sans JP");
    const QString simplified = QStringLiteral("Noto Sans SC");
    QFont font;
    font.setFamilies({QString::fromLatin1(tokens::FONT_FAMILY.data(), tokens::FONT_FAMILY.size()),
                      QStringLiteral("Noto Sans Hebrew"), chinese ? simplified : japanese,
                      chinese ? japanese : simplified, QStringLiteral("Courier New"), QStringLiteral("monospace")});
    return font;
}

Theme::Theme(QObject* parent) : QObject(parent)
{
    connect(qGuiApp, &QGuiApplication::layoutDirectionChanged, this, &Theme::rightToLeftChanged);
}

bool Theme::rightToLeft()
{
    return QGuiApplication::layoutDirection() == Qt::RightToLeft;
}

double Theme::mirrored(double x, double width, double container)
{
    return rightToLeft() ? container - x - width : x;
}

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

QColor Theme::medalGold() const
{
    return colour_of(tokens::MEDAL_GOLD);
}

QColor Theme::medalSilver() const
{
    return colour_of(tokens::MEDAL_SILVER);
}

QColor Theme::medalBronze() const
{
    return colour_of(tokens::MEDAL_BRONZE);
}

QString Theme::fontFamily() const
{
    return QString::fromLatin1(tokens::FONT_FAMILY.data(), tokens::FONT_FAMILY.size());
}

} // namespace snake::ui
