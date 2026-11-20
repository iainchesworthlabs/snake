# App Store release

This is the owner's working page for putting Snake 1.0.0 in the App Store: the order of the work, what to enter in App
Store Connect (the text is here to copy), what to check in TestFlight, and what has to be true before pressing
**Submit for Review**. The build itself is described in [Installing on an iPhone or iPad](ios-install.md) and
[Releasing](releasing.md); the signing pieces are in `docs/ios-signing.md` of `icl-common-infra`.

Nothing here has been entered in App Store Connect. The text is drafted from the game as it is on `develop`, and every
claim in it is one the [privacy](privacy.md), [features](features.md) and [how to play](how-to-play.md) pages already make.

## Where things stand

| | State |
|---|---|
| Achievements | Twelve are on `develop` since 20 November 2026, with the Achievements screen, the copy to Game Center and the twelve images. The checks of that pull request passed: the Linux builds, the sanitiser build, coverage, and an unsigned compile for iOS. Nothing of them has been seen on a device, and none is entered in App Store Connect. |
| Bundle identifier | `com.iainchesworthlabs.snake`. The app record is named **ICL Snake** (the name under the icon is Snake). |
| Signing | App Store profile, Apple Distribution certificate and the App Store Connect API key are set as secrets. `SIGN_IOS_UPLOAD=true` and `SIGN_IOS_ADHOC=true` are set; upload wins. |
| Last iOS build | Release workflow run 40599773635 (27 October 2026) built version 0.2.0, build 23, signed for the App Store, and had Apple validate it. Look in TestFlight to see whether it was uploaded. |
| On a device | The owner is trying the TestFlight build on an iPhone 17 (19 November 2026) and plans to try it on an iPad. Until results are written into the test plan below, nothing about how it plays on a device is recorded. |
| Version in the source | `0.2.0` in `CMakeLists.txt` and `vcpkg.json`. A `v1.0.0` tag stops at its first job until both say `1.0.0` and `CHANGELOG.md` has a `## [1.0.0]` section. |
| Privacy and support pages | Live at `https://iainchesworthlabs.com/snake/privacy/` and `.../support/`. |

## The order of the work

A tag builds, signs and uploads in one go, and builds are numbered by workflow run, so the same version can be uploaded
as many times as needed. That decides the order: **test with builds from `develop`, then tag the commit that passed.**

0. **Game Center** (steps 1 and 2 of "Game Center (in 1.0.0)" below, the portal and the profile) before the next signed
   build. The Game Center code is on `develop`, so a signed build fails until both are done.
1. **Prepare 1.0.0** in a pull request: the version in `CMakeLists.txt` and `vcpkg.json`, and the changelog assembled
   (`python3 <icl-common-infra>/scripts/changelog_tool.py assemble --version 1.0.0 --date YYYY-MM-DD`). Set the date to
   the day the tag is pushed. Merge it.
2. **Make the App Store Connect record** (the next section). It needs no build.
3. **Upload a TestFlight build from `develop`**, which is 1.0.0 with a new build number:

   ```sh
   gh workflow run release.yml --ref develop -f upload_testflight=true
   ```

   Nothing is published on GitHub by a run on a branch. Apple processes the build for a few minutes.
4. **Test it** on an iPhone and on an iPad (the test plan below). Each fix is a pull request to `develop` with a
   changelog fragment; upload a new build with the same command and test again. Repeat until a build passes.
5. **Check `develop` has not moved past what you tested**, then **tag the merge commit**:

   ```sh
   git tag -s v1.0.0 && git push origin v1.0.0
   ```

   You approve the run in the `release` environment. It makes the GitHub Release (a final one, so `main` is created and
   fast-forwarded to the tag), signs and attests the Linux, macOS and Windows packages, and uploads one more iOS build
   of 1.0.0 to TestFlight. **That build, not an earlier one, is the one to submit**: it is the one made from the tagged
   source, and the source link in the game's About screen and in the GitHub Release then agree with what Apple reviews.
6. **Add that build to the version** in App Store Connect, finish the record, and **Submit for Review**.
7. **After approval**, release it and update the site (the last section).

Tags and releases are the owner's by [`AGENTS.md`](https://github.com/iainchesworthlabs/snake/blob/develop/AGENTS.md):
nothing in this plan is done for you.

## The App Store Connect record

*Apps, then ICL Snake.* Enter these under **1.0 Prepare for Submission** unless a row says where.

| Field | Value |
|---|---|
| Name | ICL Snake (30 characters at most, and unique across the App Store) |
| Subtitle | A snake game with four modes (28 of 30) |
| Primary category | Games, then **Arcade** (the game's `LSApplicationCategoryType` is `public.app-category.arcade-games`) |
| Secondary category | Games, then **Action**, or none |
| Price | Free, in every storefront, or the price the owner sets. The FAQ has an `OWNER DECISION` comment for it. |
| Availability | All countries and regions, unless the owner wants fewer |
| Privacy Policy URL | `https://iainchesworthlabs.com/snake/privacy/` |
| Support URL | `https://iainchesworthlabs.com/snake/support/` |
| Marketing URL | `https://iainchesworthlabs.com/snake/` |
| Copyright | `2026 Iain Chesworth Labs contributors` (the game's own text is "Copyright (C) Iain Chesworth Labs contributors") |
| Version | 1.0.0 |
| Age rating | See below |
| Content rights | The sounds and tunes are made in code and the art is made for the game; the font and the libraries are third-party, under licences listed in the package's `THIRD-PARTY-NOTICES.txt`. Confirm that you hold the rights you need before ticking. |
| App Review contact | The owner's name, phone and email (kept by Apple, not shown) |
| Sign-in required | No |

### Promotional text (170 characters at most, can change without a new build)

> Steer by tilt, swipe or d-pad. Four modes, a top 10 for each, twelve achievements, Game Center, eight languages. No account, no ads, no tracking.

### Description (4,000 characters at most)

> Snake is a Snake game with four modes.
>
> **Four modes.** Easy has walls that wrap around, and is gentler. Classic has walls that end the game. Score 2,000 in
> Classic to open Portals, where pairs of portals carry the snake across the board. Score 3,000 to open Hot potato, where
> a bomb rolls down your body and you have to shed your tail before it goes off.
>
> **Combos and bonus items.** Eat quickly to build a combo. A bonus item turns up every so often, and goes if you wait.
>
> **Steer your way.** Tilt the phone, swipe, or use an on-screen d-pad or joystick. Tilt has a dead zone and can be
> recalibrated from the pause screen. The screen does not turn while you play.
>
> **A leaderboard for each mode.** The top 10, with the names you type, kept on your device. Sign in to Game Center and
> your scores also go on a Game Center leaderboard for each mode.
>
> **Twelve achievements.** From your first trip through the wall in Easy to 1,000 blueberries eaten. They are kept on
> your device and listed on the Achievements screen. Sign in to Game Center and the ones you earn show there too.
>
> **Sound you can set.** A volume control, and separate switches for sound and music.
>
> **Eight languages.** English, French, Spanish, German, Japanese, Simplified Chinese, Hebrew and Yiddish.
>
> **Reduced motion.** A switch turns off the shake and the bobbing food. It follows your device's setting the first time.
>
> **Your data stays yours.** No account, no ads, no analytics. The game works offline and asks for no permission; the
> only things it ever sends are your scores and the achievements you earn, to Game Center, and only if you are signed in
> to it. The source code is public under the GNU General Public License, version 3 or later.

The mode descriptions and the unlock scores come from [How to play](how-to-play.md); check them against the game before
pasting. The count of achievements and the two named in the description come from
`apps/snake/packaging/ios/game-center/components.json`.

### Keywords (100 characters at most, comma-separated, no spaces)

> `arcade,tilt,retro,classic,offline,pixel,leaderboard,casual`

(58 characters. The name and the subtitle are searched already, so "snake" is left out.)

### What's new in this version (first release)

> The first release of Snake: four modes, a leaderboard for each, twelve achievements, steering by tilt, swipe or on-screen buttons, and eight languages.

### Age rating

Answer Apple's questionnaire on the screen: its questions change from year to year. The answers that follow from the game:
no gambling, no contests, no chat or messaging, no user-generated content, no web browsing (the About screen opens the
source page in the browser, which is the system's), no purchases, no mature or sexual material, no profanity, no
medical content, no realistic violence. **Cartoon or fantasy violence is the owner's judgement:** Hot potato has a bomb
food, and a snake that meets a wall or itself flashes and ends the game. No character is shown hurt. "None" is a
defensible answer; if the owner is unsure, "Infrequent or mild" is the safer one, and it only changes the rating if
Apple's scale puts it above the lowest.

### App Privacy ("nutrition label")

*App Privacy, then Get Started.* The game has no analytics, no advertising and no account, and no code of its own or
from a third party sends anything anywhere. Two things leave the device, both to Apple's Game Center and only when the
player is signed in to it: the score of each finished game with its mode, and which achievements have been earned, with a
percentage for the two blueberry counts (the [privacy page](privacy.md) says so). **How App Store Connect wants
that declared I have not confirmed.** My understanding is that data Game Center itself handles is Apple's and the
developer does not declare it, which would leave **Data Not Collected**; if the form asks about it (gameplay content, or a
user ID, linked to the player, not used for tracking), answer what it asks and change the policy page to match. Decide
this on the screen, not from this page. The game keeps one file on the device
([Privacy](privacy.md), "What the game keeps"). Tilt reads the motion sensor while a game is on and does not keep the
readings. Apple's validation of the 0.2.0 build passed the privacy declarations of the libraries in it; check that again
on the 1.0.0 build.

### Export compliance

None to answer. The property list sets `ITSAppUsesNonExemptEncryption` to false (the game uses no encryption of its own
and does not use https), so App Store Connect does not ask.

### Notes for App Review

> Snake needs no account, sign-in, server or special hardware, and there is nothing to configure. Open the app and tap
> Play. Easy and Classic are open from the start; Portals (score 2,000 in Classic) and Hot potato (score 3,000 in
> Classic) are shown on the title screen with their targets and open when the player has scored them, so a reviewer will
> see them locked. Steering: swipe on the board, use the on-screen controls, or tilt the device. The
> game works offline, collects no data and asks for no permission. The game has twelve achievements, kept on the device
> and listed on the Achievements screen (the cup button on the title screen). Seven can be earned in Easy and Classic;
> five need Portals or Hot potato to be open, so a reviewer will see those not earned. Game Center is optional: if the player is
> signed in, the score of each game goes to the Game Center leaderboard of its mode and the achievements earned are
> reported to Game Center, and a Game Center button on the leaderboard screen and on the Achievements screen opens it;
> without it nothing changes.

If Apple asks to see Portals or Hot potato, or the five achievements that need them open, the way is to play Classic to
those scores; there is no shortcut in the build. If the owner would rather reviewers could see them, that is a change to
the game (a separate pull request), not something to put in the notes.

### Screenshots

Apple needs real screenshots of the app, taken on a device or in the Simulator. The pictures on the website are renders
of the game's phone layout on a computer and are the wrong sizes and not allowed here. Take them from the TestFlight build:
make-believe names on the leaderboard, as the website's pictures have, never a real name.

| Device | Required | Size (portrait) |
|---|---|---|
| iPhone 6.9-inch | Yes | 1290 x 2796, 1320 x 2868 or 1260 x 2736 |
| iPad 13-inch | Yes, because the app runs on iPad | 2064 x 2752 or 2048 x 2732 |

Apple has changed these sizes before. App Store Connect says what it wants when you open the section, and scales the
6.9-inch pictures down for smaller iPhones. Take up to ten of each; four or five is plenty: the title screen, a game, the
leaderboard, the Achievements screen with a few earned, Settings. The game holds the screen upright in play, and the iPad
build is full screen, so pictures in the orientation the game is shown in.

The same pictures can then go on the [press page](press.md) and in the home page card, which have `OWNER DECISION` comments
for them.

**No video**, unless the owner makes one.

### App Store badge and ID

The App Store ID is in the record already (App Store Connect, App Information, "Apple ID"); the link
`https://apps.apple.com/app/id<ID>` opens the store page only once the app is live. The badge is Apple's own SVG, from
Apple's marketing tools (`toolbox.marketingtools.apple.com`, "Download on the App Store", English, black for a light
background and white for a dark one). Use it as it is: no recolouring, no redrawing, and the clear space and minimum size
in [Apple's guidelines](https://developer.apple.com/app-store/marketing/guidelines/). Neither it nor the ID is in the
repository yet, and both go in with the pull request that follows approval ("After approval"), not before: until the app
is live the page says "Coming to the App Store".

## Game Center, accessibility and privacy

### Game Center (in 1.0.0)

The game sends the score of each finished game to a Game Center leaderboard, one for each mode, for a player who is
signed in to Game Center, and shows a Game Center button on the leaderboard screen then. The leaderboards on the device are
unchanged and nothing is sent without a sign-in. The code is in the pull request "Add Game Center leaderboards on iPhone
and iPad".

The game also has twelve achievements. The device is the record: they are earned, kept in the save file and listed on
the Achievements screen on every platform, with or without Game Center. For a player who is signed in, the game reports
each one earned to Game Center, and for the two blueberry counts a percentage on the way; Game Center shows its own
banner the first time, and a Game Center button on the Achievements screen opens Game Center's list. What was earned
while signed out or offline is reported the next time the game starts with a player signed in. The code is in the pull
request "Integrate the fixes and the achievements for 1.0.0".

Steps, in this order, because the second one gates the build:

1. **Developer portal:** Identifiers, `com.iainchesworthlabs.snake`, tick **Game Center**, save.
2. **Make the App Store provisioning profile again** for that App ID and set the repository secret
   `IOS_PROVISIONING_PROFILE_BASE64` to it (the steps are in `docs/ios-signing.md` of `icl-common-infra`). The signed build
   now carries the `com.apple.developer.game-center` entitlement, and an older profile without it stops the build when the
   app is archived. **The Game Center code is on `develop` already, so do this before the next signed build.**
3. **App Store Connect:** the app, then Game Center, turn it on. Then make four **classic** leaderboards with exactly these
   identifiers, score format integer, sort high to low, and a name in English (add other languages if wanted):

   | Mode | Leaderboard ID |
   |---|---|
   | Easy | `com.iainchesworthlabs.snake.leaderboard.easy` |
   | Classic | `com.iainchesworthlabs.snake.leaderboard.classic` |
   | Portals | `com.iainchesworthlabs.snake.leaderboard.portals` |
   | Hot potato | `com.iainchesworthlabs.snake.leaderboard.hotpotato` |

   An identifier that differs by one letter sends scores nowhere and shows nothing. App Store Connect has you add the
   leaderboards to the version you submit, and a leaderboard goes live with it; test with a sandbox Game Center account
   before then (the TestFlight plan below has the rows).
4. **App Store Connect, the twelve achievements:** the app, then Game Center, then Achievements. Everything to enter is in
   `apps/snake/packaging/ios/game-center/components.json`, and the README beside it says what each field is. For each
   achievement enter the identifier and the reference name, the points, **Hidden: No**, **Achievable more than once: No**,
   and in English the name, the description before it is earned and the description after; then upload its image,
   `apps/snake/packaging/ios/game-center/images/<image>.png` (1024 by 1024, no transparency).

   | Name | Achievement ID | Points | Image |
   |---|---|---|---|
   | Through the wall | `com.iainchesworthlabs.snake.achievement.wrap` | 10 | `wrap` |
   | Bonus item | `com.iainchesworthlabs.snake.achievement.bonus` | 10 | `bonus` |
   | Top combo | `com.iainchesworthlabs.snake.achievement.combo` | 20 | `combo` |
   | New best | `com.iainchesworthlabs.snake.achievement.best` | 20 | `best` |
   | Top speed | `com.iainchesworthlabs.snake.achievement.speed` | 30 | `speed` |
   | 100 blueberries | `com.iainchesworthlabs.snake.achievement.berries100` | 20 | `berries100` |
   | 1,000 blueberries | `com.iainchesworthlabs.snake.achievement.berries1000` | 50 | `berries1000` |
   | Portals open | `com.iainchesworthlabs.snake.achievement.portals` | 30 | `portals` |
   | Through a portal | `com.iainchesworthlabs.snake.achievement.portal` | 10 | `portal` |
   | Hot potato open | `com.iainchesworthlabs.snake.achievement.hotpotato` | 50 | `hotpotato` |
   | Shed in time | `com.iainchesworthlabs.snake.achievement.shed` | 20 | `shed` |
   | All four modes | `com.iainchesworthlabs.snake.achievement.allmodes` | 30 | `allmodes` |

   The points come to 300; Apple allows 100 for one achievement and 1,000 for an app. **An identifier cannot be changed
   once it is saved**, and one that differs by a letter from the game's is never earned in Game Center, so paste them. The
   descriptions are in `components.json` and not repeated here, so that there is one copy. English is enough for Apple;
   the game's own names and descriptions in the other seven languages are in `apps/snake/translations` (machine-made) if
   the owner wants to add them. **The first achievements of an app go to review with an app version:** add all twelve to
   the 1.0 version before submitting, as with the leaderboards. Ones added after 1.0.0 can be submitted on their own.
5. **App Privacy and the privacy page:** see above and [Privacy](privacy.md), which now says what is sent.

There is no multiplayer, no Game Center challenge or activity, and no Game Center access point; the player is asked to
sign in by Game Center itself when the game starts, and the game sees only that they did. A score sent while offline is
dropped, because the leaderboard on the device has it. An achievement is not lost that way: the game reports everything
earned on the device each time it starts with a player signed in, and under whichever player is signed in then.

### Accessibility

*App Store Connect, the app, Accessibility* (Apple's accessibility nutrition labels) lets the owner say which features
the app supports. Apple expects a feature to be declared only if people can do the app's common tasks with it, and the
repository does not show that for any of them yet:

| Feature | What the repository shows | To declare it |
|---|---|---|
| VoiceOver | Buttons, switches and mode cards carry names and roles (`Accessible.*` in `apps/snake/qml`). The board has none beyond a pane. It has never been tried with VoiceOver. | Try the whole path with VoiceOver on: title, play, pause, game over, name entry, leaderboard, Settings. Whether a game can be played with it is a real question. |
| Voice Control | Not tried | Say each button's name on a device |
| Larger Text | The game draws its own text; not checked | Set the largest text size and look at every screen |
| Dark Interface | The game has its own look and does not follow the device's | Do not declare |
| Differentiate Without Color Alone | Not checked | Look at the modes, the food, the tilt indicator and the unlock state without colour |
| Sufficient Contrast | The site's colours are measured; the game's are not | Measure the game's, or do not declare |
| Reduced Motion | A switch in Settings, set from the device's setting on a first run | Check on the device that the setting is followed and shake and flashing stop |
| Captions, Audio Descriptions | The game has no speech or video | Not applicable |

Declare nothing that has not been tried. The [accessibility page](accessibility.md) says the same ("Not tested yet"): after
the owner has tried these on the iPhone and iPad, change that page to match what was found, in the same pull request as
the labels. If Apple makes these labels required, the answer for a feature not supported is simply "not declared". Check
App Store Connect for whether they are required now.

### Privacy

- **The policy and the label** are as above: the page at `https://iainchesworthlabs.com/snake/privacy/` and the App
  Privacy answer (Data Not Collected, unless the Game Center decision above changes it). Read the page once more against
  the build you submit, in particular "What the game keeps" (the achievements earned, the blueberry count and the modes
  finished are now in the file), "What leaves your device" (the score and the achievements earned, to Game Center for a
  signed-in player, and the About screen's link, which opens the browser) and the permissions line for iPhone and iPad
  (none; Game Center asks to sign in itself, and tilt needs no permission).
- **A privacy manifest.** Apple requires an app to declare, in `PrivacyInfo.xcprivacy`, the reason for any "required
  reason" system API it or its libraries use (file times, disk space, user defaults and the like). The repository has no
  such file of its own; the game reads and writes its save file and uses Qt, which may add one to the app. Apple's
  validation of the 0.2.0 build passed. **On the build you submit, check** that the IPA holds a `PrivacyInfo.xcprivacy`
  (`unzip -l snake-...-ios-appstore.ipa | grep -i privacy`, from the `ios-appstore` artifact of the run that made it) and
  read the email Apple sends after processing, which names any missing declaration (an `ITMS-91053` warning). A missing one
  is a fix to the build, not to the store text.
- **Tracking.** None: no advertising identifier, no tracking domains, so there is no tracking prompt and the App Privacy
  tracking answer is No.
- **Age.** The name on the leaderboard is typed letters and stays on the device. Do not choose the Kids category unless the
  owner means to take on its extra rules; the game does not need it.
- **The About screen's link** opens the browser. Apple's reviewers may follow it, so the source page must be public and
  work on the day.

## TestFlight test plan

*TestFlight, then Internal Testing.* Add the owner, and anyone else with an App Store Connect login, to a group; each
installs the TestFlight app and signs in with the Apple ID that was added ([the steps](ios-install.md#for-a-tester)).
Internal builds need no Beta App Review.

On at least one iPhone and one iPad (a second, older iPhone is better). Write a result against each line; the game has not
run on a device, so expect to find things. The steering section of [Playtest and tuning](playtest.md) says what to look at
for tilt, swipe and the d-pad, and what to write down.

| Check | Result |
|---|---|
| Installs from TestFlight and opens; the icon and the name under it (Snake) are right | |
| The launch screen is the yellow of the title, not a white flash or a small window | |
| A first run on a clean install: the title, Play, a game of Easy | |
| Tilt steers the right way, on both axes; Settings, Invert axes, turns it; Recalibrate on the pause dialog works | |
| Swipe, d-pad and joystick each steer; the choice survives closing and opening the app | |
| The screen does not turn during a game; it turns on the other screens; both landscape ways on iPad, and upside down on iPad | |
| The game pauses by itself on the Home gesture, a call, the lock button, and the app switcher | |
| Safe areas: nothing under the notch, the Dynamic Island or the home indicator, on a small and a large iPhone | |
| Sound plays, the volume and the two switches work, and the silent switch behaves as expected | |
| Haptics, on an iPhone: a buzz where the game gives one, and none on an iPad | |
| A game to the end: Game over, name entry (A to Z, 12 letters), the leaderboard shows it, and it is still there after the app is killed and reopened | |
| Update over an earlier build: the leaderboards and settings survive, and a mode that was open shows its "open" achievement earned. Do not go back to the earlier build afterwards: it cannot read the new save file, and plays without saving | |
| Classic: a wall ends the game; Easy: it wraps. Unlock Portals (2,000 in Classic) and see it open | |
| Hot potato: the bomb, the Shed button, the explosion | |
| Achievements: the cup button on the title opens the Achievements screen; it shows twelve, upright and on its side, on iPhone and iPad; choosing one shows its description; the two blueberry rows show how far along they are | |
| Earn one (in Easy, go out through a wall): the Game over screen names it and plays the new-best sound; the Achievements screen shows it earned; it is still earned after the app is killed and reopened | |
| Each language (set the device's language): every screen shows with no cut-off text. Hebrew and Yiddish read right to left. | |
| Reduced motion: follows the device's setting on a first run; the switch removes shake and flashing | |
| A long session, ten minutes: it holds its frame rate, does not get hot, and does not run out of memory | |
| About: the version says 1.0.0 and the build number is the run number; the source link opens the browser | |
| VoiceOver on: each screen reads sensibly, the controls can be reached and used, and how far a game can be played is written down | |
| Largest text size and Bold Text on: nothing is cut off or overlaps (the game draws its own text, so it may not change at all, which is fine to record) | |
| Reduce Motion on in iOS Settings, then a clean install: the game's switch starts on and the shake and flashing are gone | |
| Game Center, signed in with a sandbox account: the Game Center button shows on the leaderboard screen, a finished game puts its score on that mode's leaderboard, and the button opens the right one upright and on its side, on iPhone and iPad | |
| Game Center achievements, signed in with a sandbox account: earning one shows Game Center's banner; the Game Center button on the Achievements screen opens Game Center's list, upright and on its side, on iPhone and iPad; each of the twelve there has the right name, descriptions, points and image; a blueberry count part-way shows a percentage | |
| Game Center achievements earned while signed out: sign in, start the game again, and they appear in Game Center's list | |
| Game Center, signed out (iOS Settings, Game Center): no button on the leaderboard screen or the Achievements screen, the game plays and saves as before, achievements are still earned and kept, nothing is sent; sign back in and the buttons return | |

A failure is an issue on GitHub with the device, the iOS version, the build number and what happened. A fix is a pull
request to `develop`; then upload a new build.

**What would stop a submission:** a crash on launch, a steering failure on either device family, a screen that is cut off by
the notch or the home indicator, or a leaderboard that is lost.

## Before pressing Submit for Review

- [ ] The version in the build is 1.0.0, the build is the one made by the `v1.0.0` tag, and it is attached to the version.
- [ ] Every row of the test plan has a result, and nothing on the stop list is open.
- [ ] iPhone and iPad screenshots are uploaded, from a device or the Simulator.
- [ ] The description, the keywords, the URLs and the copyright above are entered, and the three URLs open.
- [ ] The Game Center capability and profile are done, the four leaderboards exist and are on the version, and a sandbox score appeared on one; the twelve achievements exist with their images and are on the version, and one earned in the sandbox appeared in Game Center; the App Privacy answer for Game Center is decided; the accessibility labels declare only what was tried; the IPA's privacy manifest is checked.
- [ ] The age rating and App Privacy are answered, and the review contact and notes are filled in.
- [ ] Price and availability are set.
- [ ] The release option is chosen: **manually release** is the safe one, so the site and the GitHub Release can be put right the day it goes live.

## Decisions and open questions

- **Decided on 19 November 2026:** 1.0.0 is the first App Store version, so it is not held for the acceptance list on every
  platform (the roadmap says so, in the pull request that changes the version). Android and the ESP32-P4 board are later
  work, not part of this release: Android has no release and the board has no download, and the pages must not say otherwise.
- **Decided on 19 November 2026:** the owner is content with the licence (GPL-3.0-or-later) for the App Store.
- **Decided on 19 November 2026:** the achievements are built for 1.0.0. Game Center challenges and activities are designed
  and parked: nothing of them is built, and this page plans for neither.
- **The unlock scores and the achievements.** Five of the twelve achievements need Portals or Hot potato to be open, at
  2,000 and 3,000 in Classic. [Playtest and tuning](playtest.md) calls those targets an expert's and far above what a
  child scores. Until they are tuned, most players will see those five not earned. The owner decides whether to tune them
  before 1.0.0.
- **The achievement images** are drawn by `tools/game-center/make_images.py`. Nobody has seen them in Game Center's
  round crop; look at them there in the sandbox before submitting.
- **Support email.** Support is through GitHub issues only, which needs a GitHub account. Apple does not require an email,
  but a reviewer or a user who cannot sign in has no other route. The support page has an `OWNER DECISION` comment.
- **The translations** are machine-made and unread by speakers of the languages ([i18n](i18n.md)). The store text says
  "eight languages", which is true; the owner decides whether the store page should also say how they were made.
- **Price.**

## After approval

- Release the version in App Store Connect (or let it release itself).
- In `docs/index.md`: put the App Store badge and link where its `OWNER DECISION` comment is, and change "Coming to the App
  Store" and "Not out yet". Also change the same wording in `docs/features.md`, `docs/faq.md`, `docs/press.md` and
  `docs/whats-new.md` (which also has a version and date to fill in), and the `site_description` in `mkdocs.yml`.
- Put the real iPhone and iPad screenshots in the home page card and on the press page.
- Add the price to the FAQ if there is one.
- Update [Installing on an iPhone or iPad](ios-install.md), which says the game is not in the App Store.
