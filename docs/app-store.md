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
| Bundle identifier | `com.iainchesworthlabs.snake`. The app record is named **ICL Snake** (the name under the icon is Snake). |
| Signing | App Store profile, Apple Distribution certificate and the App Store Connect API key are set as secrets. `SIGN_IOS_UPLOAD=true` and `SIGN_IOS_ADHOC=true` are set; upload wins. |
| Last iOS build | Release workflow run 40599773635 (27 October 2026) built version 0.2.0, build 23, signed for the App Store, and had Apple validate it. Look in TestFlight to see whether it was uploaded. |
| On a device | The owner is trying the TestFlight build on an iPhone 17 (19 November 2026) and plans to try it on an iPad. Until results are written into the test plan below, nothing about how it plays on a device is recorded. |
| Version in the source | `0.2.0` in `CMakeLists.txt` and `vcpkg.json`. A `v1.0.0` tag stops at its first job until both say `1.0.0` and `CHANGELOG.md` has a `## [1.0.0]` section. |
| Privacy and support pages | Live at `https://iainchesworthlabs.com/snake/privacy/` and `.../support/`. |

## The order of the work

A tag builds, signs and uploads in one go, and builds are numbered by workflow run, so the same version can be uploaded
as many times as needed. That decides the order: **test with builds from `develop`, then tag the commit that passed.**

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

> Steer by tilt, swipe or d-pad. Four modes, a top 10 for each, eight languages. No account, no ads, and nothing leaves your device.

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
> **A leaderboard for each mode.** The top 10, with the names you type, kept on your device.
>
> **Sound you can set.** A volume control, and separate switches for sound and music.
>
> **Eight languages.** English, French, Spanish, German, Japanese, Simplified Chinese, Hebrew and Yiddish.
>
> **Reduced motion.** A switch turns off the shake and the bobbing food. It follows your device's setting the first time.
>
> **Your data stays yours.** No account, no ads, no analytics. The game does not use the network, and it asks for no
> permission. The source code is public under the GNU General Public License, version 3 or later.

The mode descriptions and the unlock scores come from [How to play](how-to-play.md); check them against the game before
pasting.

### Keywords (100 characters at most, comma-separated, no spaces)

> `arcade,tilt,retro,classic,offline,pixel,leaderboard,casual`

(58 characters. The name and the subtitle are searched already, so "snake" is left out.)

### What's new in this version (first release)

> The first release of Snake: four modes, a leaderboard for each, steering by tilt, swipe or on-screen buttons, and eight languages.

### Age rating

Answer Apple's questionnaire on the screen: its questions change from year to year. The answers that follow from the game:
no gambling, no contests, no chat or messaging, no user-generated content, no web browsing (the About screen opens the
source page in the browser, which is the system's), no purchases, no mature or sexual material, no profanity, no
medical content, no realistic violence. **Cartoon or fantasy violence is the owner's judgement:** Hot potato has a bomb
food, and a snake that meets a wall or itself flashes and ends the game. No character is shown hurt. "None" is a
defensible answer; if the owner is unsure, "Infrequent or mild" is the safer one, and it only changes the rating if
Apple's scale puts it above the lowest.

### App Privacy ("nutrition label")

*App Privacy, then Get Started.* **Data Not Collected.** The game has no analytics, no advertising, no account and no
network use, so nothing is collected by it or by any third-party code in it. It keeps one file on the device
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
> game does not use the network, collects no data and asks for no permission.

If Apple asks to see Portals or Hot potato, the way is to play Classic to those scores; there is no shortcut in the
build. If the owner would rather reviewers could see them, that is a change to the game (a separate pull request), not
something to put in the notes.

### Screenshots

Apple needs real screenshots of the app, taken on a device or in the Simulator. The pictures on the website are renders
of the game's phone layout on a computer and are the wrong sizes and not allowed here. Take them from the TestFlight build:
make-believe names on the leaderboard, as the website's pictures have, never a real name.

| Device | Required | Size (portrait) |
|---|---|---|
| iPhone 6.9-inch | Yes | 1290 x 2796, 1320 x 2868 or 1260 x 2736 |
| iPad 13-inch | Yes, because the app runs on iPad | 2064 x 2752 or 2048 x 2732 |

Apple has changed these sizes before. App Store Connect says what it wants when you open the section, and scales the
6.9-inch pictures down for smaller iPhones. Take up to ten of each; three or four is plenty: the title screen, a game, the
leaderboard, Settings. The game holds the screen upright in play, and the iPad build is full screen, so pictures in the
orientation the game is shown in.

The same pictures can then go on the [press page](press.md) and in the home page card, which have `OWNER DECISION` comments
for them.

**No video**, unless the owner makes one.

### App Store badge and ID

After approval the App Store ID appears in the record (and in the app's link). Apple's badge artwork is downloaded from
Apple's marketing resources and used as it is. Neither is in the repository.

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
| Update over an earlier build: the leaderboards and settings survive | |
| Classic: a wall ends the game; Easy: it wraps. Unlock Portals (2,000 in Classic) and see it open | |
| Hot potato: the bomb, the Shed button, the explosion | |
| Each language (set the device's language): every screen shows with no cut-off text. Hebrew and Yiddish read right to left. | |
| Reduced motion: follows the device's setting on a first run; the switch removes shake and flashing | |
| A long session, ten minutes: it holds its frame rate, does not get hot, and does not run out of memory | |
| About: the version says 1.0.0 and the build number is the run number; the source link opens the browser | |

A failure is an issue on GitHub with the device, the iOS version, the build number and what happened. A fix is a pull
request to `develop`; then upload a new build.

**What would stop a submission:** a crash on launch, a steering failure on either device family, a screen that is cut off by
the notch or the home indicator, or a leaderboard that is lost.

## Before pressing Submit for Review

- [ ] The version in the build is 1.0.0, the build is the one made by the `v1.0.0` tag, and it is attached to the version.
- [ ] Every row of the test plan has a result, and nothing on the stop list is open.
- [ ] iPhone and iPad screenshots are uploaded, from a device or the Simulator.
- [ ] The description, the keywords, the URLs and the copyright above are entered, and the three URLs open.
- [ ] The age rating and App Privacy (Data Not Collected) are answered, and the review contact and notes are filled in.
- [ ] Price and availability are set.
- [ ] The release option is chosen: **manually release** is the safe one, so the site and the GitHub Release can be put right the day it goes live.

## Decisions and open questions

- **Decided on 19 November 2026:** 1.0.0 is the first App Store version, so it is not held for the acceptance list on every
  platform (the roadmap says so, in the pull request that changes the version). Android has no release and the ESP32-P4
  board has no download with it, and the pages must not say otherwise.
- **Decided on 19 November 2026:** the owner is content with the licence (GPL-3.0-or-later) for the App Store.
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
