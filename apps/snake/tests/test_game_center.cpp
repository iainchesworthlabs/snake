// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/game_center.hpp"
#include "played_tally.hpp"
#include "snake/profile/achievements.hpp"
#include "snake/sim/mode.hpp"
#include "viewmodel/game_center_director.hpp"
#include "viewmodel/game_view_model.hpp"
#include "viewmodel/profile_view_model.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>
#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

using snake::profile::Achievement;
using snake::sim::Mode;
using snake::test::Played;
using snake::test::tally_of;
using snake::ui::GameCenterDirector;
using snake::ui::GameViewModel;
using snake::ui::ProfileViewModel;
using snake::ui::device::GameCenter;
using snake::ui::device::NoGameCenter;

namespace
{

class RecordingGameCenter final : public GameCenter
{
public:
    [[nodiscard]] bool available() const override
    {
        return signed_in;
    }
    void submit_score(std::string_view leaderboard_id, std::int64_t score) override
    {
        scores.emplace_back(std::string(leaderboard_id), score);
    }
    void show_leaderboard(std::string_view leaderboard_id) override
    {
        shown.emplace_back(leaderboard_id);
    }
    void report_achievement(std::string_view achievement_id, double percent) override
    {
        reports.emplace_back(std::string(achievement_id), percent);
    }
    void show_achievements() override
    {
        ++achievements_shown;
    }
    void sign_in(bool value)
    {
        signed_in = value;
        notify_change();
    }

    bool signed_in = true;
    std::vector<std::pair<std::string, std::int64_t>> scores;
    std::vector<std::string> shown;
    std::vector<std::pair<std::string, double>> reports;
    int achievements_shown = 0;
};

using Reports = std::vector<std::pair<std::string, double>>;

std::string id_of(const char* slug)
{
    return std::string("com.iainchesworthlabs.snake.achievement.") + slug;
}

/// The components file that the owner enters in App Store Connect.
QJsonObject components()
{
    QFile file(QStringLiteral(SNAKE_GAME_CENTER_COMPONENTS));
    REQUIRE(file.open(QIODevice::ReadOnly));
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    REQUIRE(document.isObject());
    return document.object();
}

std::set<std::string> ids_in(const QJsonObject& file, const char* key)
{
    std::set<std::string> ids;
    for (const QJsonValue entry : file.value(key).toArray())
    {
        ids.insert(entry.toObject().value("id").toString().toStdString());
    }
    return ids;
}

GameViewModel::SeedSource counting_seeds()
{
    return [next = std::uint64_t{1}]() mutable { return next++; };
}

// Plays a game straight on until it hits the wall (Classic).
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

TEST_CASE("Each mode has its own leaderboard identifier", "[game-center]")
{
    CHECK(GameCenterDirector::leaderboard_id(GameViewModel::Mode::Easy) ==
          "com.iainchesworthlabs.snake.leaderboard.easy");
    CHECK(GameCenterDirector::leaderboard_id(GameViewModel::Mode::Classic) ==
          "com.iainchesworthlabs.snake.leaderboard.classic");
    CHECK(GameCenterDirector::leaderboard_id(GameViewModel::Mode::Portals) ==
          "com.iainchesworthlabs.snake.leaderboard.portals");
    CHECK(GameCenterDirector::leaderboard_id(GameViewModel::Mode::HotPotato) ==
          "com.iainchesworthlabs.snake.leaderboard.hotpotato");
}

TEST_CASE("The score of a game is sent to Game Center when the game ends", "[game-center]")
{
    RecordingGameCenter game_center;
    GameViewModel game(counting_seeds());
    GameCenterDirector director(&game_center);
    director.setGame(&game);
    CHECK(director.game() == &game);

    play_into_the_wall(game);
    REQUIRE(game_center.scores.size() == 1);
    CHECK(game_center.scores.front().first == "com.iainchesworthlabs.snake.leaderboard.classic");
    CHECK(game_center.scores.front().second == game.points());
}

TEST_CASE("Nothing is sent while the player is not signed in", "[game-center]")
{
    RecordingGameCenter game_center;
    game_center.signed_in = false;
    GameViewModel game(counting_seeds());
    GameCenterDirector director(&game_center);
    director.setGame(&game);

    play_into_the_wall(game);
    CHECK(game_center.scores.empty());
    director.showLeaderboard(GameViewModel::Mode::Classic);
    CHECK(game_center.shown.empty());
}

TEST_CASE("Without a Game Center nothing is available and nothing happens", "[game-center]")
{
    GameViewModel game(counting_seeds());
    GameCenterDirector none(static_cast<GameCenter*>(nullptr));
    none.setGame(&game);
    CHECK_FALSE(none.available());
    none.showLeaderboard(GameViewModel::Mode::Easy);
    play_into_the_wall(game);

    NoGameCenter no_game_center;
    GameCenterDirector quiet(&no_game_center);
    CHECK_FALSE(quiet.available());
    quiet.showLeaderboard(GameViewModel::Mode::Easy);
}

TEST_CASE("The leaderboard of a mode is shown on request", "[game-center]")
{
    RecordingGameCenter game_center;
    GameCenterDirector director(&game_center);
    director.showLeaderboard(GameViewModel::Mode::Portals);
    REQUIRE(game_center.shown.size() == 1);
    CHECK(game_center.shown.front() == "com.iainchesworthlabs.snake.leaderboard.portals");
}

TEST_CASE("Signing in or out tells the interface", "[game-center]")
{
    RecordingGameCenter game_center;
    game_center.signed_in = false;
    GameCenterDirector director(&game_center);
    QSignalSpy spy(&director, &GameCenterDirector::availableChanged);

    CHECK_FALSE(director.available());
    game_center.sign_in(true);
    CHECK(director.available());
    CHECK(spy.count() == 1);
}

TEST_CASE("A director that has gone leaves the Game Center quiet", "[game-center]")
{
    RecordingGameCenter game_center;
    {
        GameCenterDirector director(&game_center);
    }
    game_center.sign_in(false);
    CHECK_FALSE(game_center.available());
}

TEST_CASE("Every achievement has its own identifier, and they are the ones in the components file",
          "[game-center][achievements]")
{
    std::set<std::string> in_code;
    for (std::uint8_t number = 0; number < snake::profile::ACHIEVEMENT_COUNT; ++number)
    {
        const std::string id(GameCenterDirector::achievement_id(static_cast<Achievement>(number)));
        INFO("achievement " << static_cast<int>(number));
        CHECK(id.starts_with("com.iainchesworthlabs.snake.achievement."));
        CHECK(id.size() <= 100);
        CHECK(in_code.insert(id).second);
    }
    CHECK(in_code.size() == 12);
    CHECK(GameCenterDirector::achievement_id(Achievement::WRAP) == id_of("wrap"));
    CHECK(GameCenterDirector::achievement_id(Achievement::BONUS) == id_of("bonus"));
    CHECK(GameCenterDirector::achievement_id(Achievement::COMBO) == id_of("combo"));
    CHECK(GameCenterDirector::achievement_id(Achievement::BEST) == id_of("best"));
    CHECK(GameCenterDirector::achievement_id(Achievement::SPEED) == id_of("speed"));
    CHECK(GameCenterDirector::achievement_id(Achievement::BERRIES_100) == id_of("berries100"));
    CHECK(GameCenterDirector::achievement_id(Achievement::BERRIES_1000) == id_of("berries1000"));
    CHECK(GameCenterDirector::achievement_id(Achievement::PORTALS) == id_of("portals"));
    CHECK(GameCenterDirector::achievement_id(Achievement::PORTAL) == id_of("portal"));
    CHECK(GameCenterDirector::achievement_id(Achievement::HOT_POTATO) == id_of("hotpotato"));
    CHECK(GameCenterDirector::achievement_id(Achievement::SHED) == id_of("shed"));
    CHECK(GameCenterDirector::achievement_id(Achievement::ALL_MODES) == id_of("allmodes"));

    const QJsonObject file = components();
    CHECK(ids_in(file, "achievements") == in_code);

    std::set<std::string> leaderboards;
    for (const auto mode : {GameViewModel::Mode::Easy, GameViewModel::Mode::Classic, GameViewModel::Mode::Portals,
                            GameViewModel::Mode::HotPotato})
    {
        leaderboards.emplace(GameCenterDirector::leaderboard_id(mode));
    }
    CHECK(leaderboards.size() == 4);
    CHECK(ids_in(file, "leaderboards") == leaderboards);
}

TEST_CASE("The components file stays within what Apple allows", "[game-center][achievements]")
{
    int total = 0;
    for (const QJsonValue entry : components().value("achievements").toArray())
    {
        const QJsonObject achievement = entry.toObject();
        INFO("achievement " << achievement.value("id").toString().toStdString());
        const int points = achievement.value("points").toInt(-1);
        CHECK(points > 0);
        CHECK(points <= 100);
        total += points;
        CHECK(achievement.value("hidden") == false);
        CHECK(achievement.value("achievableMoreThanOnce") == false);
        CHECK_FALSE(achievement.value("referenceName").toString().isEmpty());
        const QJsonObject english = achievement.value("en").toObject();
        CHECK_FALSE(english.value("name").toString().isEmpty());
        CHECK_FALSE(english.value("preEarnedDescription").toString().isEmpty());
        CHECK_FALSE(english.value("earnedDescription").toString().isEmpty());
    }
    CHECK(total <= 1000);
    CHECK(total == 300);
}

TEST_CASE("A game that earns two achievements reports those two at 100 and no others", "[game-center][achievements]")
{
    RecordingGameCenter game_center;
    ProfileViewModel profile;
    GameCenterDirector director(&game_center);
    director.setProfile(&profile);
    CHECK(director.profile() == &profile);
    REQUIRE(game_center.reports.empty());

    profile.record_game(Mode::CLASSIC, 100, tally_of(Played{.bonus = true, .portal = true}));
    const Reports expected{{id_of("bonus"), 100.0}, {id_of("portal"), 100.0}};
    CHECK(game_center.reports == expected);
}

TEST_CASE("A game that earns nothing reports nothing", "[game-center][achievements]")
{
    RecordingGameCenter game_center;
    ProfileViewModel profile;
    GameCenterDirector director(&game_center);
    director.setProfile(&profile);

    profile.record_game(Mode::CLASSIC, 10);
    CHECK(game_center.reports.empty());
}

TEST_CASE("A count that is not earned is reported as a whole percentage, rounded down, and not at zero",
          "[game-center][achievements]")
{
    RecordingGameCenter game_center;
    ProfileViewModel profile;
    GameCenterDirector director(&game_center);
    director.setProfile(&profile);

    // 37 of 100 is 37 and 37 of 1,000 is 3.7, which is 3.
    profile.record_game(Mode::CLASSIC, 100, tally_of(Played{.blueberries = 37}));
    Reports expected{{id_of("berries100"), 37.0}, {id_of("berries1000"), 3.0}};
    CHECK(game_center.reports == expected);

    // 4 more is 41 of 100 and 4.1 of 1,000.
    game_center.reports.clear();
    profile.record_game(Mode::CLASSIC, 100, tally_of(Played{.blueberries = 4}));
    expected = {{id_of("berries100"), 41.0}, {id_of("berries1000"), 4.0}};
    CHECK(game_center.reports == expected);

    // Below one in a hundred of the second count it is not sent.
    RecordingGameCenter other;
    ProfileViewModel fresh;
    GameCenterDirector few(&other);
    few.setProfile(&fresh);
    fresh.record_game(Mode::CLASSIC, 100, tally_of(Played{.blueberries = 9}));
    CHECK(other.reports == Reports{{id_of("berries100"), 9.0}});
}

TEST_CASE("A count that is earned is reported at 100 and the other as its percentage", "[game-center][achievements]")
{
    RecordingGameCenter game_center;
    ProfileViewModel profile;
    GameCenterDirector director(&game_center);
    director.setProfile(&profile);

    profile.record_game(Mode::CLASSIC, 100, tally_of(Played{.blueberries = 999}));
    const Reports expected{{id_of("berries100"), 100.0}, {id_of("berries1000"), 99.0}};
    CHECK(game_center.reports == expected);

    // The next game does not send the earned one again, and the other is earned.
    game_center.reports.clear();
    profile.record_game(Mode::CLASSIC, 100, tally_of(Played{.blueberries = 1}));
    CHECK(game_center.reports == Reports{{id_of("berries1000"), 100.0}});
}

TEST_CASE("Nothing is reported while the player is not signed in", "[game-center][achievements]")
{
    RecordingGameCenter game_center;
    game_center.signed_in = false;
    ProfileViewModel profile;
    GameCenterDirector director(&game_center);
    director.setProfile(&profile);

    profile.record_game(Mode::CLASSIC, 100, tally_of(Played{.blueberries = 37, .bonus = true}));
    CHECK(game_center.reports.empty());
    director.showAchievements();
    CHECK(game_center.achievements_shown == 0);
}

TEST_CASE("What was earned while signed out is reported when the player signs in", "[game-center][achievements]")
{
    RecordingGameCenter game_center;
    game_center.signed_in = false;
    ProfileViewModel profile;
    GameCenterDirector director(&game_center);
    director.setProfile(&profile);
    profile.record_game(Mode::CLASSIC, 100, tally_of(Played{.blueberries = 37, .bonus = true}));
    REQUIRE(game_center.reports.empty());

    game_center.sign_in(true);
    const Reports expected{{id_of("bonus"), 100.0}, {id_of("berries100"), 37.0}, {id_of("berries1000"), 3.0}};
    CHECK(game_center.reports == expected);

    // Signing in again sends it again, which does no harm.
    game_center.reports.clear();
    game_center.sign_in(true);
    CHECK(game_center.reports == expected);

    // Signing out sends nothing.
    game_center.reports.clear();
    game_center.sign_in(false);
    CHECK(game_center.reports.empty());
}

TEST_CASE("A profile that already holds achievements reports them once Game Center is available",
          "[game-center][achievements]")
{
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    const QString path = folder.filePath("save.snk");
    {
        ProfileViewModel earlier;
        earlier.setSaveFile(path);
        earlier.record_game(Mode::CLASSIC, 100, tally_of(Played{.blueberries = 37, .bonus = true}));
    }
    const Reports expected{{id_of("bonus"), 100.0}, {id_of("berries100"), 37.0}, {id_of("berries1000"), 3.0}};

    SECTION("a profile that is set after its save file was loaded")
    {
        RecordingGameCenter game_center;
        ProfileViewModel profile;
        profile.setSaveFile(path);
        GameCenterDirector director(&game_center);
        director.setProfile(&profile);
        CHECK(game_center.reports == expected);
    }

    SECTION("a save file that is loaded after the profile was set")
    {
        RecordingGameCenter game_center;
        ProfileViewModel profile;
        GameCenterDirector director(&game_center);
        director.setProfile(&profile);
        REQUIRE(game_center.reports.empty());
        profile.setSaveFile(path);
        CHECK(game_center.reports == expected);
    }

    SECTION("a player who signs in after the save file was loaded")
    {
        RecordingGameCenter game_center;
        game_center.signed_in = false;
        ProfileViewModel profile;
        profile.setSaveFile(path);
        GameCenterDirector director(&game_center);
        director.setProfile(&profile);
        REQUIRE(game_center.reports.empty());
        game_center.sign_in(true);
        CHECK(game_center.reports == expected);
    }
}

TEST_CASE("A profile with nothing earned and nothing counted reports nothing, and a profile let go is not heard",
          "[game-center][achievements]")
{
    RecordingGameCenter game_center;
    ProfileViewModel profile;
    GameCenterDirector director(&game_center);
    director.setProfile(&profile);
    game_center.sign_in(true);
    CHECK(game_center.reports.empty());

    director.setProfile(nullptr);
    profile.record_game(Mode::CLASSIC, 100, tally_of(Played{.bonus = true}));
    CHECK(game_center.reports.empty());
}

TEST_CASE("The achievements are shown on request, and only while signed in", "[game-center][achievements]")
{
    RecordingGameCenter game_center;
    GameCenterDirector director(&game_center);
    director.showAchievements();
    CHECK(game_center.achievements_shown == 1);

    game_center.sign_in(false);
    director.showAchievements();
    CHECK(game_center.achievements_shown == 1);

    NoGameCenter none;
    GameCenterDirector quiet(&none);
    quiet.showAchievements();
    GameCenterDirector nothing(static_cast<GameCenter*>(nullptr));
    nothing.showAchievements();
}
