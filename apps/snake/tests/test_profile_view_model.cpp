// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/profile/unlocks.hpp"
#include "snake/sim/mode.hpp"
#include "viewmodel/game_view_model.hpp"
#include "viewmodel/leaderboard_model.hpp"
#include "viewmodel/mode_list_model.hpp"
#include "viewmodel/profile_view_model.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QByteArray>
#include <QChar>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QLocale>
#include <QModelIndex>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <cstdint>
#include <utility>

using snake::sim::Mode;
using snake::ui::GameViewModel;
using snake::ui::ModeListModel;
using snake::ui::ProfileViewModel;
using Entry = ModeListModel::Entry;

namespace
{

QVariant value(const ModeListModel& model, Entry entry, ModeListModel::Role role)
{
    return model.data(model.index(static_cast<int>(entry)), role);
}

GameViewModel::SeedSource counting_seeds()
{
    return [next = std::uint64_t{1}]() mutable { return next++; };
}

// Numbers in the hints are written for the player's locale; the tests fix one.
struct UsLocale
{
    QLocale previous;

    UsLocale() : previous(QLocale())
    {
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    }
    UsLocale(const UsLocale&) = delete;
    UsLocale& operator=(const UsLocale&) = delete;
    ~UsLocale()
    {
        QLocale::setDefault(previous);
    }
};

// Types `name` and saves the score that waits for it.
void save_as(ProfileViewModel& profile, const QString& name)
{
    for (const QChar letter : name)
    {
        profile.typeCharacter(QString(letter));
    }
    REQUIRE(profile.saveScore());
}

// Plays a Classic game straight on until it hits the wall.
void play_into_the_wall(GameViewModel& game)
{
    game.start();
    for (int i = 0; i < 20 && game.state() == GameViewModel::State::Running; ++i)
    {
        game.advance(0.15);
    }
    REQUIRE(game.state() == GameViewModel::State::GameOver);
}

} // namespace

TEST_CASE("The mode list has the four modes in order", "[modes]")
{
    ProfileViewModel profile;
    const ModeListModel& modes = *profile.modes();
    REQUIRE(modes.rowCount() == 4);
    CHECK(modes.rowCount(modes.index(0)) == 0);
    CHECK(value(modes, Entry::Easy, ModeListModel::NameRole).toString() == "Easy");
    CHECK(value(modes, Entry::Classic, ModeListModel::NameRole).toString() == "Classic");
    CHECK(value(modes, Entry::Portals, ModeListModel::NameRole).toString() == "Portals");
    CHECK(value(modes, Entry::HotPotato, ModeListModel::NameRole).toString() == "Hot potato");
    CHECK(value(modes, Entry::Easy, ModeListModel::EntryRole).toInt() == static_cast<int>(Entry::Easy));
    CHECK(modes.roleNames().size() == 7);
}

TEST_CASE("Easy and Classic are open and can be started, with their rules in words", "[modes]")
{
    ProfileViewModel profile;
    const ModeListModel& modes = *profile.modes();
    for (const Entry entry : {Entry::Easy, Entry::Classic})
    {
        CHECK_FALSE(value(modes, entry, ModeListModel::LockedRole).toBool());
        CHECK(value(modes, entry, ModeListModel::SelectableRole).toBool());
    }
    CHECK(value(modes, Entry::Easy, ModeListModel::GameModeRole).toInt() ==
          static_cast<int>(GameViewModel::Mode::Easy));
    CHECK(value(modes, Entry::Classic, ModeListModel::GameModeRole).toInt() ==
          static_cast<int>(GameViewModel::Mode::Classic));
    CHECK(value(modes, Entry::Easy, ModeListModel::DetailRole).toString() == "Slower snake. Walls wrap around.");
    CHECK(value(modes, Entry::Classic, ModeListModel::DetailRole).toString() == "Faster snake. Walls end the game.");
}

TEST_CASE("Portals and Hot potato are locked, with what it takes to open them, and cannot be started", "[modes]")
{
    const UsLocale locale;
    ProfileViewModel profile;
    const ModeListModel& modes = *profile.modes();
    for (const Entry entry : {Entry::Portals, Entry::HotPotato})
    {
        CHECK(value(modes, entry, ModeListModel::LockedRole).toBool());
        CHECK_FALSE(value(modes, entry, ModeListModel::SelectableRole).toBool());
        CHECK(value(modes, entry, ModeListModel::GameModeRole).toInt() == -1);
    }
    CHECK(value(modes, Entry::Portals, ModeListModel::DetailRole).toString() == "Unlocks at 2,000 in Classic");
    CHECK(value(modes, Entry::HotPotato, ModeListModel::DetailRole).toString() == "Unlocks at 3,000 in Classic");
}

TEST_CASE("An unknown row or role gives nothing", "[modes]")
{
    ProfileViewModel profile;
    const ModeListModel& modes = *profile.modes();
    CHECK_FALSE(modes.data(QModelIndex{}, ModeListModel::NameRole).isValid());
    CHECK_FALSE(modes.data(modes.index(4), ModeListModel::NameRole).isValid());
    CHECK_FALSE(modes.data(modes.index(0), Qt::DisplayRole).isValid());
}

TEST_CASE("A score that opens a mode unlocks it for good and says so", "[profile]")
{
    ProfileViewModel profile;
    const ModeListModel& modes = *profile.modes();
    QSignalSpy changed(profile.modes(), &ModeListModel::dataChanged);
    QSignalSpy result(&profile, &ProfileViewModel::resultChanged);

    profile.record_game(Mode::CLASSIC, 2000);
    CHECK(profile.unlockedNow() == QStringList{"Portals"});
    CHECK(profile.data().unlocks.has(snake::profile::Unlockable::PORTALS));
    CHECK_FALSE(profile.data().unlocks.has(snake::profile::Unlockable::HOT_POTATO));
    CHECK(changed.count() == 1);
    CHECK(result.count() == 1);
    // Open, but not in the game yet: neither locked nor startable.
    CHECK_FALSE(value(modes, Entry::Portals, ModeListModel::LockedRole).toBool());
    CHECK_FALSE(value(modes, Entry::Portals, ModeListModel::SelectableRole).toBool());
    CHECK(value(modes, Entry::Portals, ModeListModel::DetailRole).toString() == "Coming in a later version.");
    CHECK(value(modes, Entry::HotPotato, ModeListModel::LockedRole).toBool());

    // A worse game later does not close it, and does not announce it again.
    profile.record_game(Mode::CLASSIC, 10);
    CHECK(profile.unlockedNow().isEmpty());
    CHECK_FALSE(value(modes, Entry::Portals, ModeListModel::LockedRole).toBool());
    CHECK(changed.count() == 1);

    profile.record_game(Mode::CLASSIC, 3500);
    CHECK(profile.unlockedNow() == QStringList{"Hot potato"});
}

TEST_CASE("Easy scores do not open modes that are earned in Classic", "[profile]")
{
    ProfileViewModel profile;
    profile.record_game(Mode::EASY, 5000);
    CHECK(profile.unlockedNow().isEmpty());
    CHECK(profile.data().unlocks.none());
}

TEST_CASE("The best score is kept for each mode", "[profile]")
{
    GameViewModel game(counting_seeds());
    ProfileViewModel profile;
    profile.setGame(&game);
    CHECK(profile.best() == "000000");

    // A score that waits for its name already counts as the best.
    profile.record_game(Mode::CLASSIC, 420);
    CHECK(profile.best() == "000420");
    save_as(profile, "MUM");
    CHECK(profile.best() == "000420");
    // The best shown is the one of the mode being played.
    game.setMode(GameViewModel::Mode::Easy);
    CHECK(profile.best() == "000000");
    profile.record_game(Mode::EASY, 90);
    save_as(profile, "DAD");
    CHECK(profile.best() == "000090");
    game.setMode(GameViewModel::Mode::Classic);
    CHECK(profile.best() == "000420");
}

TEST_CASE("A game says whether it beat the best and by how far it fell short", "[profile]")
{
    ProfileViewModel profile;

    // Nothing scored on an empty record: no best to compare with.
    profile.record_game(Mode::CLASSIC, 0);
    CHECK_FALSE(profile.newBest());
    CHECK(profile.pointsToBest() == -1);

    profile.record_game(Mode::CLASSIC, 1480);
    CHECK(profile.newBest());
    CHECK(profile.pointsToBest() == 0);
    save_as(profile, "MUM");

    // The figures of the design: 420 against a best of 1480 is 1060 short.
    profile.record_game(Mode::CLASSIC, 420);
    CHECK_FALSE(profile.newBest());
    CHECK(profile.pointsToBest() == 1060);

    // A tie is not a new best.
    profile.record_game(Mode::CLASSIC, 1480);
    CHECK_FALSE(profile.newBest());
    CHECK(profile.pointsToBest() == 0);

    // The other mode has its own record.
    profile.record_game(Mode::EASY, 30);
    CHECK(profile.newBest());
}

TEST_CASE("The profile notes each game the view-model finishes, once", "[profile]")
{
    GameViewModel game(counting_seeds());
    ProfileViewModel profile;
    QSignalSpy result(&profile, &ProfileViewModel::resultChanged);
    QSignalSpy best(&profile, &ProfileViewModel::bestChanged);
    profile.setGame(&game);
    CHECK(profile.game() == &game);
    CHECK(best.count() == 1);

    play_into_the_wall(game);
    CHECK(result.count() == 1);
    CHECK(profile.best() == game.score());

    // The change of state to a new game and back to the running state records nothing.
    game.restart();
    CHECK(result.count() == 1);
}

TEST_CASE("Changing the game stops the profile watching the old one", "[profile]")
{
    GameViewModel first(counting_seeds());
    GameViewModel second(counting_seeds());
    ProfileViewModel profile;
    QSignalSpy result(&profile, &ProfileViewModel::resultChanged);

    profile.setGame(&first);
    profile.setGame(&first);
    profile.setGame(&second);
    play_into_the_wall(first);
    CHECK(result.count() == 0);
    play_into_the_wall(second);
    CHECK(result.count() == 1);

    profile.setGame(nullptr);
    CHECK(profile.best() == "000000");
}

TEST_CASE("A score that makes the top 10 waits for a name", "[profile][name]")
{
    ProfileViewModel profile;
    CHECK_FALSE(profile.qualifies());
    CHECK(profile.pendingRank() == 0);
    CHECK(profile.pendingScore().isEmpty());
    CHECK(profile.standing().isEmpty());

    // A score of nothing never gets on.
    profile.record_game(Mode::CLASSIC, 0);
    CHECK_FALSE(profile.qualifies());

    QSignalSpy pending(&profile, &ProfileViewModel::pendingChanged);
    profile.record_game(Mode::CLASSIC, 420);
    CHECK(profile.qualifies());
    CHECK(profile.pendingScore() == "000420");
    CHECK(profile.pendingRank() == 1);
    CHECK(pending.count() == 1);
    // Nothing is on the board until it is saved.
    CHECK(profile.data().leaderboards.of(Mode::CLASSIC).empty());
    CHECK_FALSE(profile.canSave());
}

TEST_CASE("The name is typed a letter at a time in capitals, up to twelve", "[profile][name]")
{
    ProfileViewModel profile;
    // Without a waiting score there is nothing to type into.
    profile.typeCharacter("A");
    CHECK(profile.entryName().isEmpty());

    profile.record_game(Mode::EASY, 100);
    QSignalSpy entry(&profile, &ProfileViewModel::entryChanged);
    profile.typeCharacter("m");
    profile.typeCharacter("A");
    CHECK(profile.entryName() == "MA");
    CHECK(entry.count() == 2);

    // Anything but a letter or a space is left out, and so is more than one character at a time.
    profile.typeCharacter("7");
    profile.typeCharacter("!");
    profile.typeCharacter("\xC3\xA4");
    profile.typeCharacter("");
    profile.typeCharacter("YA");
    CHECK(profile.entryName() == "MA");
    CHECK(entry.count() == 2);

    for (int i = 0; i < 20; ++i)
    {
        profile.typeCharacter("Z");
    }
    CHECK(profile.entryName() == "MAZZZZZZZZZZ");
    CHECK(profile.entryName().size() == profile.nameLength());

    profile.eraseCharacter();
    CHECK(profile.entryName() == "MAZZZZZZZZZ");
}

TEST_CASE("A name can be saved once it has a letter in it", "[profile][name]")
{
    ProfileViewModel profile;
    profile.record_game(Mode::EASY, 100);
    CHECK_FALSE(profile.canSave());
    CHECK_FALSE(profile.saveScore());

    profile.typeCharacter(" ");
    profile.typeCharacter(" ");
    CHECK_FALSE(profile.canSave());
    CHECK_FALSE(profile.saveScore());
    CHECK(profile.qualifies());

    profile.typeCharacter("a");
    CHECK(profile.canSave());

    // Taking the letter off again closes the way.
    profile.eraseCharacter();
    CHECK_FALSE(profile.canSave());
    profile.eraseCharacter();
    profile.eraseCharacter();
    profile.eraseCharacter();
    CHECK(profile.entryName().isEmpty());
}

TEST_CASE("Saving puts the score on the leaderboard under the name without its outer spaces", "[profile][name]")
{
    ProfileViewModel profile([] { return std::int64_t{1'700'000'000}; });
    profile.record_game(Mode::CLASSIC, 1480);
    QSignalSpy pending(&profile, &ProfileViewModel::pendingChanged);
    QSignalSpy recent(&profile, &ProfileViewModel::recentNamesChanged);

    for (const char letter : {' ', 'm', 'u', 'm', ' '})
    {
        profile.typeCharacter(QString(QChar::fromLatin1(letter)));
    }
    REQUIRE(profile.saveScore());

    const auto& board = profile.data().leaderboards.of(Mode::CLASSIC);
    REQUIRE(board.size() == 1);
    CHECK(board.at(0).name.view() == "MUM");
    CHECK(board.at(0).score == 1480);
    CHECK(board.at(0).unix_seconds == 1'700'000'000);
    CHECK(profile.data().leaderboards.of(Mode::EASY).empty());
    CHECK(profile.recentNames() == QStringList{"MUM"});
    CHECK(recent.count() == 1);

    CHECK_FALSE(profile.qualifies());
    CHECK(profile.pendingRank() == 0);
    CHECK(pending.count() == 1);
    CHECK(profile.entryName().isEmpty());
    CHECK(profile.best() == "000000");
    // Nothing is left to save.
    CHECK_FALSE(profile.saveScore());
}

TEST_CASE("The time of the game is taken when it ends, not when the name is saved", "[profile][name]")
{
    std::int64_t now = 1000;
    ProfileViewModel profile([&now] { return now; });
    profile.record_game(Mode::EASY, 50);
    now = 5000;
    save_as(profile, "A");
    CHECK(profile.data().leaderboards.of(Mode::EASY).at(0).unix_seconds == 1000);
}

TEST_CASE("The three latest names are kept, newest first, and a name is kept once", "[profile][name]")
{
    ProfileViewModel profile;
    for (const char* name : {"MUM", "DAD", "LEO", "MAYA", "DAD"})
    {
        profile.record_game(Mode::EASY, 10);
        save_as(profile, name);
    }
    CHECK(profile.recentNames() == QStringList{"DAD", "MAYA", "LEO"});
}

TEST_CASE("A recent name replaces the name typed", "[profile][name]")
{
    ProfileViewModel profile;
    profile.record_game(Mode::EASY, 10);
    save_as(profile, "MUM");
    profile.record_game(Mode::EASY, 20);
    save_as(profile, "DAD");

    profile.record_game(Mode::EASY, 30);
    profile.typeCharacter("X");
    profile.chooseRecent(1);
    CHECK(profile.entryName() == "MUM");
    CHECK(profile.canSave());
    profile.chooseRecent(0);
    CHECK(profile.entryName() == "DAD");

    // A place with no name, or none at all, changes nothing.
    profile.chooseRecent(2);
    profile.chooseRecent(-1);
    CHECK(profile.entryName() == "DAD");
    REQUIRE(profile.saveScore());
    CHECK(profile.data().leaderboards.of(Mode::EASY).at(0).name.view() == "DAD");
    CHECK(profile.recentNames() == QStringList{"DAD", "MUM"});

    // With no score waiting there is nothing to put a name in.
    profile.chooseRecent(0);
    CHECK(profile.entryName().isEmpty());
}

TEST_CASE("Erasing from an empty name does nothing", "[profile][name]")
{
    ProfileViewModel profile;
    profile.record_game(Mode::EASY, 10);
    QSignalSpy entry(&profile, &ProfileViewModel::entryChanged);
    profile.eraseCharacter();
    CHECK(entry.count() == 0);
}

TEST_CASE("The standing shows the place the score will take among its neighbours", "[profile][name]")
{
    ProfileViewModel profile;
    for (const auto& [name, score] : {std::pair{"MUM", 1480}, std::pair{"DAD", 1310}, std::pair{"LEO", 980}})
    {
        profile.record_game(Mode::CLASSIC, static_cast<std::uint32_t>(score));
        save_as(profile, name);
    }

    // The design's example: third place, between DAD and LEO, with the name as far as it is typed.
    profile.record_game(Mode::CLASSIC, 1280);
    CHECK(profile.pendingRank() == 3);
    profile.typeCharacter("M");
    profile.typeCharacter("A");
    const QVariantList standing = profile.standing();
    REQUIRE(standing.size() == 3);
    const auto at = [&](int index) { return standing.at(index).toMap(); };
    CHECK(at(0).value("place") == 2);
    CHECK(at(0).value("name") == "DAD");
    CHECK(at(0).value("score") == "001310");
    CHECK_FALSE(at(0).value("mine").toBool());
    CHECK(at(1).value("place") == 3);
    CHECK(at(1).value("name") == "MA");
    CHECK(at(1).value("score") == "001280");
    CHECK(at(1).value("mine").toBool());
    CHECK(at(2).value("place") == 4);
    CHECK(at(2).value("name") == "LEO");
    CHECK_FALSE(at(2).value("mine").toBool());
}

TEST_CASE("The standing at the top has no line above and at the bottom none below", "[profile][name]")
{
    ProfileViewModel profile;
    profile.record_game(Mode::CLASSIC, 500);
    save_as(profile, "MUM");

    profile.record_game(Mode::CLASSIC, 900);
    QVariantList standing = profile.standing();
    REQUIRE(standing.size() == 2);
    CHECK(standing.at(0).toMap().value("mine").toBool());
    CHECK(standing.at(0).toMap().value("place") == 1);
    CHECK(standing.at(1).toMap().value("name") == "MUM");
    CHECK(standing.at(1).toMap().value("place") == 2);

    profile.record_game(Mode::CLASSIC, 100);
    standing = profile.standing();
    REQUIRE(standing.size() == 2);
    CHECK(standing.at(0).toMap().value("name") == "MUM");
    CHECK(standing.at(1).toMap().value("mine").toBool());
    CHECK(standing.at(1).toMap().value("place") == 2);
}

TEST_CASE("On a full board the last place has nothing below it and a tie does not get on", "[profile][name]")
{
    ProfileViewModel profile;
    for (std::uint32_t score = 100; score <= 1000; score += 100)
    {
        profile.record_game(Mode::EASY, score);
        save_as(profile, "AAA");
    }
    REQUIRE(profile.data().leaderboards.of(Mode::EASY).size() == 10);

    // Equal to the 10th entry: the earlier one ranks higher, so this does not qualify.
    profile.record_game(Mode::EASY, 100);
    CHECK_FALSE(profile.qualifies());
    CHECK(profile.standing().isEmpty());

    // Better than the 10th: it takes its place, and the old 10th falls off, so there is no line below.
    profile.record_game(Mode::EASY, 150);
    CHECK(profile.pendingRank() == 10);
    const QVariantList standing = profile.standing();
    REQUIRE(standing.size() == 2);
    CHECK(standing.at(1).toMap().value("mine").toBool());
    save_as(profile, "BBB");
    const auto& board = profile.data().leaderboards.of(Mode::EASY);
    CHECK(board.size() == 10);
    CHECK(board.at(9).score == 150);
}

TEST_CASE("A score that is not saved can be let go, and the next game replaces it", "[profile][name]")
{
    ProfileViewModel profile;
    profile.record_game(Mode::CLASSIC, 700);
    profile.discardScore();
    CHECK_FALSE(profile.qualifies());
    CHECK(profile.best() == "000000");
    profile.discardScore();

    profile.record_game(Mode::CLASSIC, 800);
    profile.typeCharacter("A");
    profile.record_game(Mode::CLASSIC, 900);
    CHECK(profile.pendingScore() == "000900");
    CHECK(profile.entryName().isEmpty());
    // The game that was never saved does not count as the best to beat.
    CHECK(profile.newBest());
}

TEST_CASE("Without a save file the profile lives in memory", "[profile][save]")
{
    ProfileViewModel profile;
    CHECK(profile.saveFile().isEmpty());
    CHECK_FALSE(profile.persistent());
    profile.setSaveFile("");
    CHECK_FALSE(profile.persistent());
}

TEST_CASE("Unlocks, names and scores come back from the save file", "[profile][save]")
{
    const QTemporaryDir folder;
    REQUIRE(folder.isValid());
    const QString path = folder.filePath("deeper/save.snk");

    {
        ProfileViewModel profile;
        QSignalSpy changed(&profile, &ProfileViewModel::saveFileChanged);
        profile.setSaveFile(path);
        CHECK(changed.count() == 1);
        CHECK(profile.saveFile() == path);
        CHECK(profile.persistent());
        // The same file again is no change.
        profile.setSaveFile(path);
        CHECK(changed.count() == 1);

        // A game that opens a mode is written at once, before any name is typed.
        profile.record_game(Mode::CLASSIC, 2100);
        CHECK(QFile::exists(path));
        save_as(profile, "MAYA");
        profile.record_game(Mode::EASY, 40);
        save_as(profile, "LEO");
    }

    ProfileViewModel again;
    again.setSaveFile(path);
    CHECK(again.persistent());
    CHECK(again.data().unlocks.has(snake::profile::Unlockable::PORTALS));
    CHECK(again.recentNames() == QStringList{"LEO", "MAYA"});
    REQUIRE(again.data().leaderboards.of(Mode::CLASSIC).size() == 1);
    CHECK(again.data().leaderboards.of(Mode::CLASSIC).at(0).name.view() == "MAYA");
    CHECK(again.data().leaderboards.of(Mode::CLASSIC).at(0).score == 2100);
    CHECK(again.data().leaderboards.of(Mode::EASY).at(0).name.view() == "LEO");
    // The mode list was told, so Portals is open on the title screen.
    CHECK_FALSE(value(*again.modes(), Entry::Portals, ModeListModel::LockedRole).toBool());

    GameViewModel game(counting_seeds());
    game.setMode(GameViewModel::Mode::Classic);
    again.setGame(&game);
    CHECK(again.best() == "002100");
}

TEST_CASE("A first run has no file and starts afresh", "[profile][save]")
{
    const QTemporaryDir folder;
    ProfileViewModel profile;
    profile.setSaveFile(folder.filePath("save.snk"));
    CHECK(profile.persistent());
    CHECK(profile.recentNames().isEmpty());
    CHECK_FALSE(QFile::exists(folder.filePath("save.snk")));
}

TEST_CASE("A damaged save file is put aside and the profile starts afresh", "[profile][save]")
{
    const QTemporaryDir folder;
    const QString path = folder.filePath("save.snk");
    {
        QFile file(path);
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write("this is not a save file");
    }

    ProfileViewModel profile;
    profile.setSaveFile(path);
    CHECK(profile.persistent());
    CHECK(profile.recentNames().isEmpty());
    CHECK_FALSE(QFile::exists(path));
    CHECK(QFile::exists(path + ".damaged"));

    profile.record_game(Mode::EASY, 10);
    save_as(profile, "A");
    CHECK(QFile::exists(path));
}

TEST_CASE("A save file from a newer version is left as it is", "[profile][save]")
{
    const QTemporaryDir folder;
    const QString path = folder.filePath("save.snk");
    const QByteArray newer("SNKS\x63\x00more than this program knows", 32);
    {
        QFile file(path);
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write(newer);
    }

    ProfileViewModel profile;
    profile.setSaveFile(path);
    CHECK_FALSE(profile.persistent());
    profile.record_game(Mode::EASY, 10);
    save_as(profile, "A");

    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    CHECK(file.readAll() == newer);
    CHECK_FALSE(QFile::exists(path + ".damaged"));
}

TEST_CASE("A save file that cannot be read is left alone", "[profile][save]")
{
    const QTemporaryDir folder;
    // A folder in the place of the file: it exists and cannot be opened for reading.
    REQUIRE(QDir(folder.path()).mkdir("save.snk"));

    ProfileViewModel profile;
    profile.setSaveFile(folder.filePath("save.snk"));
    CHECK_FALSE(profile.persistent());
}

TEST_CASE("A save that cannot be written does not stop the game", "[profile][save]")
{
    const QTemporaryDir folder;
    // The folder of the save file is a file, so it can never be made.
    QFile blocker(folder.filePath("blocker"));
    REQUIRE(blocker.open(QIODevice::WriteOnly));
    blocker.close();

    ProfileViewModel profile;
    profile.setSaveFile(folder.filePath("blocker/save.snk"));
    CHECK(profile.persistent());
    profile.record_game(Mode::EASY, 10);
    save_as(profile, "A");
    CHECK(profile.data().leaderboards.of(Mode::EASY).size() == 1);
    CHECK_FALSE(QFile::exists(folder.filePath("blocker/save.snk")));
}

TEST_CASE("Choosing another file drops the score that waited for a name", "[profile][save]")
{
    const QTemporaryDir folder;
    ProfileViewModel profile;
    profile.record_game(Mode::EASY, 10);
    REQUIRE(profile.qualifies());
    profile.setSaveFile(folder.filePath("save.snk"));
    CHECK_FALSE(profile.qualifies());
}

namespace
{

QVariant line_value(const snake::ui::LeaderboardModel& model, int row, snake::ui::LeaderboardModel::Role role)
{
    return model.data(model.index(row), role);
}

} // namespace

TEST_CASE("The leaderboard model lists the lines of the mode shown, best first", "[profile][leaderboard]")
{
    using snake::ui::LeaderboardModel;
    ProfileViewModel profile;
    const LeaderboardModel& board = *profile.leaderboard();
    CHECK(board.rowCount() == 0);
    CHECK(board.rowCount(board.index(0)) == 0);
    CHECK(board.roleNames().size() == 4);

    profile.record_game(Mode::EASY, 300);
    save_as(profile, "LEO");
    profile.record_game(Mode::CLASSIC, 1480);
    save_as(profile, "MUM");
    profile.record_game(Mode::CLASSIC, 1310);
    save_as(profile, "DAD");

    // The leaderboard is on the mode just played.
    CHECK(profile.boardMode() == GameViewModel::Mode::Classic);
    REQUIRE(board.rowCount() == 2);
    CHECK(line_value(board, 0, LeaderboardModel::PlaceRole) == 1);
    CHECK(line_value(board, 0, LeaderboardModel::NameRole) == "MUM");
    CHECK(line_value(board, 0, LeaderboardModel::ScoreRole) == "001480");
    CHECK(line_value(board, 1, LeaderboardModel::PlaceRole) == 2);
    CHECK(line_value(board, 1, LeaderboardModel::NameRole) == "DAD");

    QSignalSpy mode(&profile, &ProfileViewModel::boardModeChanged);
    profile.setBoardMode(GameViewModel::Mode::Easy);
    CHECK(mode.count() == 1);
    profile.setBoardMode(GameViewModel::Mode::Easy);
    CHECK(mode.count() == 1);
    REQUIRE(board.rowCount() == 1);
    CHECK(line_value(board, 0, LeaderboardModel::NameRole) == "LEO");
}

TEST_CASE("Only the line saved last is marked, and only on its own mode's board", "[profile][leaderboard]")
{
    using snake::ui::LeaderboardModel;
    ProfileViewModel profile;
    const LeaderboardModel& board = *profile.leaderboard();
    profile.record_game(Mode::CLASSIC, 500);
    save_as(profile, "MUM");
    profile.record_game(Mode::CLASSIC, 900);
    save_as(profile, "DAD");

    // DAD went in above MUM.
    CHECK(line_value(board, 0, LeaderboardModel::NameRole) == "DAD");
    CHECK(line_value(board, 0, LeaderboardModel::LatestRole).toBool());
    CHECK_FALSE(line_value(board, 1, LeaderboardModel::LatestRole).toBool());

    profile.setBoardMode(GameViewModel::Mode::Easy);
    CHECK(board.rowCount() == 0);
}

TEST_CASE("The leaderboard model gives nothing for a row that is not there", "[profile][leaderboard]")
{
    using snake::ui::LeaderboardModel;
    ProfileViewModel profile;
    profile.record_game(Mode::EASY, 10);
    save_as(profile, "A");
    const LeaderboardModel& board = *profile.leaderboard();
    CHECK_FALSE(board.data(QModelIndex{}, LeaderboardModel::NameRole).isValid());
    CHECK_FALSE(board.data(board.index(1), LeaderboardModel::NameRole).isValid());
    CHECK_FALSE(board.data(board.index(0), Qt::DisplayRole).isValid());
}

TEST_CASE("The leaderboard comes back from the save file with no line marked", "[profile][leaderboard]")
{
    using snake::ui::LeaderboardModel;
    const QTemporaryDir folder;
    const QString path = folder.filePath("save.snk");
    {
        ProfileViewModel profile;
        profile.setSaveFile(path);
        profile.record_game(Mode::EASY, 70);
        save_as(profile, "MAYA");
    }
    ProfileViewModel again;
    again.setSaveFile(path);
    again.setBoardMode(GameViewModel::Mode::Easy);
    const LeaderboardModel& board = *again.leaderboard();
    REQUIRE(board.rowCount() == 1);
    CHECK(line_value(board, 0, LeaderboardModel::NameRole) == "MAYA");
    CHECK_FALSE(line_value(board, 0, LeaderboardModel::LatestRole).toBool());
}
