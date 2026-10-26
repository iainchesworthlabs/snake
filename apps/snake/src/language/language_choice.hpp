// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace snake::ui
{

/// The languages the game has a catalogue for, by the code that names its file (snake_<code>.qm): the same as
/// apps/snake/translations. English is the source language and the fallback.
[[nodiscard]] QStringList available_languages();

/// What was chosen and why.
struct LanguageChoice
{
    /// One of available_languages().
    QString code;
    /// The tag of the preferred language it came from, as the system gave it, or empty when none matched and English is
    /// the fallback.
    QString matched;
};

/// Picks the language of the game from the languages the person prefers, most wanted first, as the system lists them
/// (`en-GB`, `de-DE`, `zh-Hans-CN`, `en_AU`, and the old `iw` for Hebrew). The first preference that the game has a
/// catalogue for wins, and it is matched by its language alone: any English (`en-GB`, `en-AU`, `en-Latn-US`) is
/// English, any German is German. So a phone that lists English (UK), German and French gets English. Chinese is
/// Simplified
/// (`zh_CN`) unless the tag says Traditional (a Hant script, or Taiwan, Hong Kong or Macao), which the game does not
/// have and passes over. With nothing that matches, English.
///
/// The choice is made here and not left to QTranslator and the system's list, because what a system lists differs: some
/// give `en-GB` and some give `en-Latn-GB, en-GB, en-Latn, en`.
[[nodiscard]] LanguageChoice choose_language(const QStringList& preferred, const QStringList& available);

/// The choice the program made at start-up and what the system offered, for About to show. Empty until set.
void record_language_choice(const LanguageChoice& choice, const QStringList& preferred);
[[nodiscard]] QString language_choice_description();

} // namespace snake::ui
