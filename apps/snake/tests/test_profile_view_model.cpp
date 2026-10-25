// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/profile/unlocks.hpp"
#include "snake/sim/mode.hpp"
#include "viewmodel/game_view_model.hpp"
#include "viewmodel/mode_list_model.hpp"
#include "viewmodel/profile_view_model.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QLocale>
#include <QModelIndex>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <cstdint>

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

    profile.record_game(Mode::CLASSIC, 420);
    CHECK(profile.best() == "000420");
    // The best shown is the one of the mode being played.
    game.setMode(GameViewModel::Mode::Easy);
    CHECK(profile.best() == "000000");
    profile.record_game(Mode::EASY, 90);
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
