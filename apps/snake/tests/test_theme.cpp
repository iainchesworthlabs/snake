// SPDX-License-Identifier: GPL-3.0-or-later
#include "theme/theme.hpp"
#include "theme/tokens.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QByteArray>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocale>
#include <QRawFont>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QXmlStreamReader>
#include <algorithm>
#include <cstdint>
#include <vector>

namespace tokens = snake::ui::tokens;
using snake::ui::Theme;

namespace
{

struct ColourToken
{
    const char* name;
    std::uint32_t value;
};

const std::vector<ColourToken>& colour_tokens()
{
    static const std::vector<ColourToken> all = {
        {"background", tokens::BACKGROUND},
        {"ink", tokens::INK},
        {"shadow", tokens::SHADOW},
        {"surface", tokens::SURFACE},
        {"accent", tokens::ACCENT},
        {"boardLight", tokens::BOARD_LIGHT},
        {"boardDark", tokens::BOARD_DARK},
        {"food", tokens::FOOD},
        {"foodHighlight", tokens::FOOD_HIGHLIGHT},
        {"leaf", tokens::LEAF},
        {"tongue", tokens::TONGUE},
        {"eye", tokens::EYE},
        {"medalGold", tokens::MEDAL_GOLD},
        {"medalSilver", tokens::MEDAL_SILVER},
        {"medalBronze", tokens::MEDAL_BRONZE},
    };
    return all;
}

QJsonObject read_tokens()
{
    QFile file(QStringLiteral(SNAKE_TOKENS_JSON));
    REQUIRE(file.open(QIODevice::ReadOnly));
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    REQUIRE(document.isObject());
    return document.object();
}

} // namespace

TEST_CASE("every colour token matches docs/design/tokens.json", "[theme]")
{
    const QJsonObject colours = read_tokens().value(QStringLiteral("color")).toObject();
    for (const auto& token : colour_tokens())
    {
        INFO(token.name);
        const QString text = colours.value(QString::fromLatin1(token.name)).toObject().value("value").toString();
        REQUIRE(QColor(text).isValid());
        CHECK(QColor(text).rgb() == QColor::fromRgb(token.value).rgb());
    }
}

TEST_CASE("every opaque colour in tokens.json has a constant", "[theme]")
{
    const QJsonObject colours = read_tokens().value(QStringLiteral("color")).toObject();
    QStringList known;
    for (const auto& token : colour_tokens())
    {
        known.push_back(QString::fromLatin1(token.name));
    }
    for (auto it = colours.begin(); it != colours.end(); ++it)
    {
        const QString text = it.value().toObject().value("value").toString();
        // The overlay is the only colour with an alpha channel and is drawn with its own rule.
        if (text.startsWith(QLatin1Char('#')))
        {
            INFO(it.key().toStdString());
            CHECK(known.contains(it.key()));
        }
    }
}

TEST_CASE("the overlay is the ink colour at the opacity of tokens.json", "[theme]")
{
    const QJsonObject colours = read_tokens().value(QStringLiteral("color")).toObject();
    const QString text = colours.value(QStringLiteral("overlay")).toObject().value("value").toString();
    // "rgba(r,g,b,a)" with the alpha as a fraction.
    REQUIRE(text.startsWith(QStringLiteral("rgba(")));
    const QStringList parts = text.mid(5, text.size() - 6).split(QLatin1Char(','));
    REQUIRE(parts.size() == 4);
    const QColor expected = QColor::fromRgb(parts[0].toInt(), parts[1].toInt(), parts[2].toInt());
    CHECK(expected.rgb() == QColor::fromRgb(tokens::INK).rgb());
    CHECK(tokens::OVERLAY_ALPHA == Catch::Approx(parts[3].toDouble()));

    const QColor overlay = Theme().overlay();
    CHECK(overlay.rgb() == expected.rgb());
    CHECK(overlay.alphaF() == Catch::Approx(parts[3].toDouble()).margin(1.0 / 255.0));
}

TEST_CASE("the font family constant is the family of tokens.json", "[theme]")
{
    const QJsonObject typography = read_tokens().value(QStringLiteral("typography")).toObject();
    CHECK(typography.value(QStringLiteral("family")).toString().toStdString() == std::string(tokens::FONT_FAMILY));
}

TEST_CASE("the bundled fonts load and the application font resolves to them", "[theme]")
{
    REQUIRE(snake::ui::load_fonts());
    const QString family = QString::fromLatin1(tokens::FONT_FAMILY.data(), tokens::FONT_FAMILY.size());
    REQUIRE(QFontDatabase::families().contains(family));

    // All four weights are present.
    const QStringList styles = QFontDatabase::styles(family);
    CHECK(styles.size() >= 4);

    const QFont font = snake::ui::application_font();
    CHECK(font.families().first() == family);
    CHECK(QFontInfo(font).family() == family);
}

TEST_CASE("the theme gives the colours and the family to QML", "[theme]")
{
    const Theme theme;
    CHECK(theme.background() == QColor::fromRgb(tokens::BACKGROUND));
    CHECK(theme.ink() == QColor::fromRgb(tokens::INK));
    CHECK(theme.shadow() == QColor::fromRgb(tokens::SHADOW));
    CHECK(theme.surface() == QColor::fromRgb(tokens::SURFACE));
    CHECK(theme.accent() == QColor::fromRgb(tokens::ACCENT));
    CHECK(theme.fontFamily() == QStringLiteral("Pixelify Sans"));
}

TEST_CASE("the medal colours are the tokens of the leaderboard tiles", "[theme]")
{
    const Theme theme;
    CHECK(theme.medalGold().rgb() == QColor::fromRgb(tokens::MEDAL_GOLD).rgb());
    CHECK(theme.medalSilver().rgb() == QColor::fromRgb(tokens::MEDAL_SILVER).rgb());
    CHECK(theme.medalBronze().rgb() == QColor::fromRgb(tokens::MEDAL_BRONZE).rgb());
}

TEST_CASE("the font of the language comes first for the characters Japanese and Chinese share", "[theme][fonts]")
{
    const QStringList japanese = snake::ui::application_font(QLocale(QLocale::Japanese)).families();
    CHECK(japanese.at(0) == QString::fromLatin1(tokens::FONT_FAMILY.data(), tokens::FONT_FAMILY.size()));
    CHECK(japanese.at(1) == "Noto Sans Hebrew");
    CHECK(japanese.at(2) == "Noto Sans JP");
    CHECK(japanese.at(3) == "Noto Sans SC");
    const QStringList chinese = snake::ui::application_font(QLocale(QLocale::Chinese, QLocale::China)).families();
    CHECK(chinese.at(2) == "Noto Sans SC");
    CHECK(chinese.at(3) == "Noto Sans JP");
    // Every other language gets the same fallbacks, and the monospace of the tokens last.
    CHECK(snake::ui::application_font(QLocale(QLocale::French)).families().last() == "monospace");
}

namespace
{

/// The characters of every translation in the catalogue of `language`.
QSet<char32_t> characters_of(const QString& language)
{
    QFile file(QDir(QStringLiteral(SNAKE_TRANSLATIONS_DIR)).filePath(QStringLiteral("snake_%1.ts").arg(language)));
    REQUIRE(file.open(QIODevice::ReadOnly));
    QXmlStreamReader xml(&file);
    QSet<char32_t> found;
    while (!xml.atEnd())
    {
        if (xml.readNext() == QXmlStreamReader::StartElement && xml.name() == u"translation")
        {
            for (const char32_t character : xml.readElementText().toStdU32String())
            {
                found.insert(character);
            }
        }
    }
    REQUIRE_FALSE(xml.hasError());
    return found;
}

} // namespace

TEST_CASE("the bundled fonts cover every character of every translation", "[theme][fonts]")
{
    REQUIRE(snake::ui::load_fonts());
    // The fonts the game carries: the pixel font and the fallbacks. A character the system could supply does not count,
    // because the game must look the same on every machine.
    std::vector<QRawFont> fonts;
    for (const QString& family : {QString::fromLatin1(tokens::FONT_FAMILY.data(), tokens::FONT_FAMILY.size()),
                                  QString("Noto Sans JP"), QString("Noto Sans SC"), QString("Noto Sans Hebrew")})
    {
        if (QFontDatabase::families().contains(family))
        {
            fonts.push_back(QRawFont::fromFont(QFont(family)));
        }
    }
    REQUIRE(fonts.size() >= 3);

    for (const QString& language : QStringList{"en", "zh_CN", "fr", "es", "de", "ja", "he", "yi"})
    {
        if (!QFile::exists(
                QDir(QStringLiteral(SNAKE_TRANSLATIONS_DIR)).filePath(QStringLiteral("snake_%1.ts").arg(language))))
        {
            continue;
        }
        for (const char32_t character : characters_of(language))
        {
            if (QChar::isSpace(character) || character == U'\n')
            {
                continue;
            }
            INFO("language " << language.toStdString() << " character U+"
                             << QString::number(character, 16).toStdString());
            CHECK(std::ranges::any_of(fonts,
                                      [character](const QRawFont& font) { return font.supportsCharacter(character); }));
        }
    }
}
