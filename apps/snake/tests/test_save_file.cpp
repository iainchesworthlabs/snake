// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/save_file.hpp"
#include "snake/profile/player_name.hpp"
#include "snake/profile/save_data.hpp"
#include "snake/sim/mode.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

using snake::profile::SaveData;
using snake::ui::SaveFile;

namespace
{

SaveData sample()
{
    SaveData data;
    data.settings.invert_axes = true;
    data.recent_names.remember(*snake::profile::PlayerName::from_text("MAYA"));
    data.leaderboards.of(snake::sim::Mode::CLASSIC)
        .insert({.name = *snake::profile::PlayerName::from_text("MAYA"), .score = 1280, .unix_seconds = 1'700'000'000});
    return data;
}

void write(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write(bytes);
}

} // namespace

TEST_CASE("Saved data reads back the same", "[save-file]")
{
    const QTemporaryDir folder;
    const SaveFile file(folder.filePath("save.snk"));
    REQUIRE(file.save(sample()));
    const auto loaded = file.load();
    REQUIRE(loaded);
    CHECK(*loaded == sample());
}

TEST_CASE("A save creates the folder and replaces the file whole", "[save-file]")
{
    const QTemporaryDir folder;
    const SaveFile file(folder.filePath("a/b/save.snk"));
    REQUIRE(file.save(SaveData{}));
    REQUIRE(file.save(sample()));
    CHECK(*file.load() == sample());
    // No temporary file stays behind.
    CHECK(QDir(folder.filePath("a/b")).entryList(QDir::Files) == QStringList{"save.snk"});
}

TEST_CASE("A file that is not there is the first run", "[save-file]")
{
    const QTemporaryDir folder;
    const SaveFile file(folder.filePath("save.snk"));
    const auto loaded = file.load();
    REQUIRE_FALSE(loaded);
    CHECK(loaded.error() == SaveFile::Error::MISSING);
}

TEST_CASE("A damaged file is moved aside, replacing an older damaged one", "[save-file]")
{
    const QTemporaryDir folder;
    const SaveFile file(folder.filePath("save.snk"));
    CHECK(file.damaged_path() == folder.filePath("save.snk.damaged"));
    write(file.damaged_path(), "an older casualty");

    write(file.path(), "garbage");
    const auto loaded = file.load();
    REQUIRE_FALSE(loaded);
    CHECK(loaded.error() == SaveFile::Error::DAMAGED);
    CHECK_FALSE(QFile::exists(file.path()));
    QFile moved(file.damaged_path());
    REQUIRE(moved.open(QIODevice::ReadOnly));
    CHECK(moved.readAll() == "garbage");

    // The next load finds nothing, as on a first run.
    CHECK(file.load().error() == SaveFile::Error::MISSING);
}

TEST_CASE("A file with one byte changed is damaged", "[save-file]")
{
    const QTemporaryDir folder;
    const SaveFile file(folder.filePath("save.snk"));
    REQUIRE(file.save(sample()));
    QByteArray bytes;
    {
        QFile in(file.path());
        REQUIRE(in.open(QIODevice::ReadOnly));
        bytes = in.readAll();
    }
    bytes[12] = static_cast<char>(bytes[12] ^ 0x01);
    write(file.path(), bytes);
    CHECK(file.load().error() == SaveFile::Error::DAMAGED);
}

TEST_CASE("A file from a newer version is reported and left in place", "[save-file]")
{
    const QTemporaryDir folder;
    const SaveFile file(folder.filePath("save.snk"));
    write(file.path(), QByteArray("SNKS\x63\x00from the future", 21));
    CHECK(file.load().error() == SaveFile::Error::NEWER_VERSION);
    CHECK(QFile::exists(file.path()));
    CHECK_FALSE(QFile::exists(file.damaged_path()));
}

TEST_CASE("A folder in the place of the file cannot be read", "[save-file]")
{
    const QTemporaryDir folder;
    REQUIRE(QDir(folder.path()).mkdir("save.snk"));
    const SaveFile file(folder.filePath("save.snk"));
    CHECK(file.load().error() == SaveFile::Error::UNREADABLE);
}

TEST_CASE("A damaged file that cannot be moved aside is unreadable", "[save-file]")
{
    const QTemporaryDir folder;
    const SaveFile file(folder.filePath("save.snk"));
    write(file.path(), "garbage");
    // A folder is in the way of the new name.
    REQUIRE(QDir(folder.path()).mkdir("save.snk.damaged"));
    write(folder.filePath("save.snk.damaged/keep"), "x");
    CHECK(file.load().error() == SaveFile::Error::UNREADABLE);
}

TEST_CASE("A save that cannot reach its folder reports it", "[save-file]")
{
    const QTemporaryDir folder;
    write(folder.filePath("blocker"), "a file where a folder should be");
    const SaveFile file(folder.filePath("blocker/save.snk"));
    const auto saved = file.save(sample());
    REQUIRE_FALSE(saved);
    CHECK(saved.error() == SaveFile::Error::UNREADABLE);
}

TEST_CASE("A save over a folder reports it and leaves the folder", "[save-file]")
{
    const QTemporaryDir folder;
    REQUIRE(QDir(folder.path()).mkdir("save.snk"));
    const SaveFile file(folder.filePath("save.snk"));
    CHECK(file.save(sample()).error() == SaveFile::Error::UNREADABLE);
    CHECK(QDir(folder.filePath("save.snk")).exists());
}
