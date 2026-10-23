// SPDX-License-Identifier: GPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

// The Qt Linguist catalogues in apps/snake/translations/snake_<code>.ts, read as text so that this
// test needs no Qt and runs on every leg. lupdate writes one element per line in a fixed shape, so a
// plain string walk is enough. The checks are the ones a reader of a catalogue cannot see at a
// glance: a string one language lacks, an entry left unfinished or obsolete, an empty translation and
// a dropped or changed placeholder (%1, %n). The standard lists more languages; adding one is a
// change to LANGUAGES and a new file.

namespace
{

constexpr std::array<std::string_view, 2> LANGUAGES{"en", "zh_CN"};

struct Message
{
    std::string source;
    std::string attributes;                // inside <translation ...>
    std::vector<std::string> translations; // one entry, or one per numerus form
};

std::string unescape(std::string text)
{
    constexpr std::array<std::pair<std::string_view, std::string_view>, 5> ENTITIES{
        {{"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&apos;", "'"}, {"&amp;", "&"}}};
    for (const auto& [from, to] : ENTITIES)
    {
        for (auto at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
        {
            text.replace(at, from.size(), to);
        }
    }
    return text;
}

// The text between open and close, searching from `from` and not past `until`.
std::string between(const std::string& text, std::size_t from, std::size_t until, std::string_view open,
                    std::string_view close)
{
    const auto start = text.find(open, from);
    if (start == std::string::npos || start >= until)
    {
        return {};
    }
    const auto begin = start + open.size();
    return text.substr(begin, text.find(close, begin) - begin);
}

// The test runs in apps/snake/translations (see tests/CMakeLists.txt), so the files are found by name.
std::filesystem::path catalogue_path(std::string_view code)
{
    return std::filesystem::path("snake_" + std::string(code) + ".ts");
}

std::vector<Message> read_catalogue(std::string_view code)
{
    const auto path = catalogue_path(code);
    INFO("catalogue " << path.string());
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.is_open());
    // Not an istreambuf_iterator range: GCC 13 and 14 report a null dereference inside <streambuf> for it.
    std::string text(std::filesystem::file_size(path), '\0');
    in.read(text.data(), static_cast<std::streamsize>(text.size()));

    std::vector<Message> messages;
    for (auto at = text.find("<message"); at != std::string::npos; at = text.find("<message", at + 1))
    {
        const auto end = text.find("</message>", at);
        REQUIRE(end != std::string::npos);
        Message message;
        message.source = unescape(between(text, at, end, "<source>", "</source>"));

        const auto tag = text.find("<translation", at);
        REQUIRE(tag < end);
        const auto tag_end = text.find('>', tag);
        message.attributes = text.substr(tag + 12, tag_end - tag - 12);
        if (!message.attributes.empty() && message.attributes.back() == '/')
        {
            message.translations.emplace_back();
        }
        else if (const auto first = text.find("<numerusform", tag); first != std::string::npos && first < end)
        {
            for (auto form = first; form != std::string::npos && form < end; form = text.find("<numerusform", form + 1))
            {
                message.translations.push_back(unescape(between(text, form, end, ">", "</numerusform>").substr(0)));
            }
        }
        else
        {
            message.translations.push_back(unescape(between(text, tag, end, ">", "</translation>")));
        }
        messages.push_back(std::move(message));
    }
    return messages;
}

// The placeholders of a string, sorted: "%1 of %2" gives {"%1", "%2"}.
std::multiset<std::string> placeholders(const std::string& text)
{
    std::multiset<std::string> found;
    for (auto at = text.find('%'); at != std::string::npos; at = text.find('%', at + 1))
    {
        if (at + 1 < text.size() &&
            (std::isdigit(static_cast<unsigned char>(text[at + 1])) != 0 || text[at + 1] == 'n'))
        {
            auto end = at + 1;
            while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end])) != 0)
            {
                ++end;
            }
            found.insert(text.substr(at, std::max<std::size_t>(end - at, 2)));
        }
    }
    return found;
}

} // namespace

TEST_CASE("Every language holds the same source strings", "[translations]")
{
    std::set<std::string> reference;
    for (const auto& message : read_catalogue(LANGUAGES.front()))
    {
        reference.insert(message.source);
    }
    REQUIRE_FALSE(reference.empty());

    for (const auto code : LANGUAGES)
    {
        INFO("language " << code);
        std::set<std::string> sources;
        for (const auto& message : read_catalogue(code))
        {
            sources.insert(message.source);
        }
        CHECK(sources == reference);
    }
}

TEST_CASE("No translation is unfinished, obsolete or empty", "[translations]")
{
    for (const auto code : LANGUAGES)
    {
        for (const auto& message : read_catalogue(code))
        {
            INFO("language " << code << ", source \"" << message.source << "\"");
            CHECK(message.attributes.find("unfinished") == std::string::npos);
            CHECK(message.attributes.find("obsolete") == std::string::npos);
            CHECK(message.attributes.find("vanished") == std::string::npos);
            for (const auto& translation : message.translations)
            {
                CHECK_FALSE(translation.empty());
            }
        }
    }
}

TEST_CASE("A translation keeps the placeholders of its source", "[translations]")
{
    for (const auto code : LANGUAGES)
    {
        for (const auto& message : read_catalogue(code))
        {
            INFO("language " << code << ", source \"" << message.source << "\"");
            for (const auto& translation : message.translations)
            {
                CHECK(placeholders(translation) == placeholders(message.source));
            }
        }
    }
}

TEST_CASE("The window title and the frame label are translated", "[translations]")
{
    std::map<std::string, std::string> chinese;
    for (const auto& message : read_catalogue("zh_CN"))
    {
        chinese[message.source] = message.translations.front();
    }
    REQUIRE(chinese.contains("Snake"));
    REQUIRE(chinese.contains("Frame %1"));
    CHECK(chinese["Snake"] != "Snake");
    CHECK(chinese["Frame %1"] != "Frame %1");
}
