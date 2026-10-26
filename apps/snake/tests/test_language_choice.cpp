// SPDX-License-Identifier: GPL-3.0-or-later
#include "language/language_choice.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QString>
#include <QStringList>

using snake::ui::available_languages;
using snake::ui::choose_language;

namespace
{

QString chosen(const QStringList& preferred)
{
    return choose_language(preferred, available_languages()).code;
}

} // namespace

TEST_CASE("Any English is English", "[language]")
{
    for (const char* tag : {"en", "en-GB", "en-AU", "en-US", "en_AU", "en-Latn-GB", "EN-gb", "en-CA", "en-IN", "en-ZA"})
    {
        INFO(tag);
        CHECK(chosen({QString::fromUtf8(tag)}) == "en");
    }
}

TEST_CASE("English first on a phone that lists English (UK), German and French gets English", "[language]")
{
    CHECK(chosen({"en-GB", "de-DE", "fr-FR"}) == "en");
    CHECK(chosen({"en-AU", "de-DE", "fr-FR"}) == "en");
    // The long form some systems give, with the fallbacks written out.
    CHECK(chosen({"en-Latn-GB", "en-GB", "en-Latn", "en", "de-Latn-DE", "de-DE", "de", "fr-FR"}) == "en");
    const auto choice = choose_language({"en-GB", "de-DE"}, available_languages());
    CHECK(choice.matched == "en-GB");
}

TEST_CASE("The first language that has a catalogue wins, in the order the person gave", "[language]")
{
    CHECK(chosen({"de-DE", "en-GB"}) == "de");
    CHECK(chosen({"fr-CA", "de-DE"}) == "fr");
    CHECK(chosen({"es-419"}) == "es");
    CHECK(chosen({"ja-JP"}) == "ja");
    // One the game does not have is passed over, not an end of the search.
    CHECK(chosen({"it-IT", "nl-NL", "de-AT"}) == "de");
    CHECK(chosen({"pt-BR", "es-MX", "en-US"}) == "es");
}

TEST_CASE("The generic language matches whatever region comes after it", "[language]")
{
    for (const char* tag : {"de", "de-DE", "de-AT", "de-CH", "de-Latn-DE", "de_CH"})
    {
        INFO(tag);
        CHECK(chosen({QString::fromUtf8(tag)}) == "de");
    }
    for (const char* tag : {"fr", "fr-FR", "fr-CA", "fr-BE", "fr-CH"})
    {
        INFO(tag);
        CHECK(chosen({QString::fromUtf8(tag)}) == "fr");
    }
}

TEST_CASE("Chinese is the Simplified one unless the tag says Traditional", "[language]")
{
    for (const char* tag : {"zh", "zh-CN", "zh-Hans", "zh-Hans-CN", "zh-SG", "zh_CN", "zh-Hans-SG"})
    {
        INFO(tag);
        CHECK(chosen({QString::fromUtf8(tag)}) == "zh_CN");
    }
    // The game has no Traditional text: the next language the person gave, or English.
    CHECK(chosen({"zh-Hant-TW", "en-GB"}) == "en");
    CHECK(chosen({"zh-TW", "de-DE"}) == "de");
    CHECK(chosen({"zh-HK"}) == "en");
    CHECK(chosen({"zh-Hant"}) == "en");
    CHECK(chosen({"zh-MO", "ja-JP"}) == "ja");
}

TEST_CASE("Hebrew and Yiddish are found under their old codes too", "[language]")
{
    CHECK(chosen({"he-IL"}) == "he");
    CHECK(chosen({"iw"}) == "he");
    CHECK(chosen({"iw-IL"}) == "he");
    CHECK(chosen({"yi"}) == "yi");
    CHECK(chosen({"ji"}) == "yi");
}

TEST_CASE("With nothing the game has, or nothing at all, English", "[language]")
{
    CHECK(chosen({"it-IT", "nl-NL", "ko-KR"}) == "en");
    CHECK(chosen({}) == "en");
    CHECK(chosen({""}) == "en");
    CHECK(chosen({"-", "_"}) == "en");
    const auto choice = choose_language({"ko-KR"}, available_languages());
    CHECK(choice.code == "en");
    CHECK(choice.matched.isEmpty());
}

TEST_CASE("A language that is not in the list given is not chosen", "[language]")
{
    CHECK(choose_language({"de-DE", "fr-FR"}, {"en", "fr"}).code == "fr");
    CHECK(choose_language({"de-DE"}, {"en"}).code == "en");
}

TEST_CASE("Every catalogue of the game can be reached by its own code", "[language]")
{
    for (const QString& code : available_languages())
    {
        INFO(code.toStdString());
        CHECK(chosen({code}) == code);
    }
}

TEST_CASE("The choice is described for About", "[language]")
{
    const auto choice = choose_language({"en-GB", "de-DE"}, available_languages());
    snake::ui::record_language_choice(choice, {"en-GB", "de-DE"});
    CHECK(snake::ui::language_choice_description() == "en (from en-GB; the system offers en-GB, de-DE)");
    snake::ui::record_language_choice(choose_language({}, available_languages()), {});
    CHECK(snake::ui::language_choice_description() == "en (nothing matched; the system offers nothing)");
}
