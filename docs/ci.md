# Continuous integration

The workflows in `.github/workflows/` call reusable workflows in `icl-common-infra`, pinned by
commit SHA. Dependabot proposes the SHA bump; the pull request also needs
`tools/ci/sync_cmake.py` run, because the CMake modules are vendored.

## What runs

| Event | Jobs |
|---|---|
| Draft pull request | Policy, Plan, Site, workflow analysis, dependency scan |
| Ready pull request | Above, plus Linux release and sanitizer builds and coverage |
| Merge queue, push to `develop` | Above, plus Linux debug and TSan, macOS, Windows release and ASan |
| Nightly | CodeQL |
| Tag `v*` | Release |

The Plan job reads `.github/ci/plan.json`. A change to documentation only builds nothing. A path
the plan does not recognise builds everything.

## The ESP32-P4 libraries

`esp32-libraries.yml` builds the portable libraries (`libs/sim`, `libs/input`, `libs/audio`, `libs/profile`) with Espressif's
RISC-V toolchain for the ESP32-P4, under the project's warnings as errors and with exceptions and RTTI off, on two compilers:
the one of the ESP-IDF release the firmware is built with (`gate_toolchain`, GCC 15.2 for ESP-IDF v6.1) and the compiler floor
(`floor_toolchain`, GCC 16.1). Both, with the SHA-256 of their archives, are in the `esp32p4` entry of
`.github/toolchain-versions.json`; `tools/esp32/compile_libraries.sh` downloads one, checks the hash before it unpacks it,
and compiles the 21 sources, one object each. Nothing is linked or run: there is no board in CI. Like the phone builds it is
not a required check and runs only when a library, the script or the pins change; run it by hand anywhere with
`gh workflow run esp32-libraries.yml --ref <branch>`. To try it on a computer: `tools/esp32/compile_libraries.sh
<toolchain> <sha256> [directory]`.

## Phones and tablets

Two workflows, `android-build.yml` and `ios-build.yml`, build the game for a device and look inside what they made. They
are **not** required checks and not on the merge queue: they run on a pull request or a push only when something the
build reads changes, because a required check that a path filter keeps from running never reports, and because the Qt kits
are several gigabytes and a macOS runner costs a multiple. Run one by hand anywhere with
`gh workflow run android-build.yml --ref <branch>`.

- **Android** (Linux runner): Qt's Android kit and the Linux host kit from aqtinstall, the NDK, platform and build tools
  of `.github/toolchain-versions.json` (`android.*`) from the runner's SDK, the runner's JDK 17, the vcpkg toolchain
  chained to Qt's, and the `qtsensors` module. The `android-arm64-debuggable` preset makes a debug-signed APK, and
  `verify_apk.sh` reads it: the application id, the version, the platform levels, **no permission but the vibrator**,
  `arm64-v8a` only, the library, the icon and the
  signature. The APK is uploaded for 14 days.
- **iOS** (the `xcode-27` runner, Xcode 27 and the iOS 27 SDK): Qt's iOS kit and the macOS host kit, the `ios-release` preset
  through Xcode, unsigned, and `verify_app.sh` reads the bundle, which fails a build made with an SDK older than 27
  (`DTSDKName`). The signed build of `release.yml` runs on the same runner.

What they have **not** shown, and a device has to: that the program starts, that it draws at the size of the screen, that
touch and tilt work, that the sound plays and the audio session behaves, and that a package installs. Nothing runs the
program here: there is no emulator job, because a hosted emulator needs hardware acceleration and an image download that
is not reliable enough to stand behind a green tick.

Signing is the owner's. These two workflows use no secret.

**The signed iPhone and iPad build** is the `ios` job of `release.yml`. It and the `Game Center` workflow (below) are the
only places a secret is used for iOS. It is off until the owner sets a repository variable: `SIGN_IOS_UPLOAD=true` for
**TestFlight**, or `SIGN_IOS_ADHOC=true` for an
ad-hoc IPA. It imports the Apple Distribution certificate and this repository's provisioning profile with
`import-ios-signing`, configures with `SNAKE_IOS_SIGNING_TEAM`, `SNAKE_IOS_SIGNING_PROFILE` (both read out of the profile; see
`mobile.cmake` for why the application target alone is signed) and `SNAKE_IOS_BUILD_NUMBER` (the number of the run), archives
and exports with `export-ios-ipa`, and checks the IPA with `verify_app.sh --signed` (`--store` for TestFlight). For TestFlight
it then has Apple validate the build and, for a tag or a manual run with `upload_testflight`, uploads it
(`upload-testflight`). The actions are in `icl-common-infra` (`docs/ios-signing.md` there says where each secret comes
from). A run on a branch (`gh workflow run release.yml --ref <branch>`) publishes nothing. An ad-hoc IPA is the workflow
artifact `ios-adhoc` for 7 days and goes to the release only if `IOS_IPA_ON_RELEASE` is `true`; it carries the UDIDs of the
devices, and neither place is private in a public repository. How to install is in
[Installing on an iPhone or iPad](ios-install.md). The Android upload key is a later step.

### Game Center

`game-center.yml` keeps Game Center in App Store Connect the same as
`apps/snake/packaging/ios/game-center/components.json`: the four leaderboards and the twelve achievements, with their
names, descriptions and images. The file is the record, and `tools/game-center/sync_app_store_connect.py` does the work
with the App Store Connect API key the signed build uses (`APPSTORE_API_KEY_ID`, `APPSTORE_API_ISSUER_ID`,
`APPSTORE_API_PRIVATE_KEY`). It needs Python 3 and `openssl`, which the runner has, and nothing else. It is **not** a
required check, for the reason above.

| When | What runs |
|---|---|
| A pull request that changes the file, an image or the script | **Check the file and the images.** No secret is used. |
| A push to `develop` that changes one of them | The check, then **Say what would change**: it asks Apple what is there and writes the difference in the summary of the run. If there is one, **Make the changes** waits in the `release` environment, and runs when the owner approves it. |
| `gh workflow run game-center.yml` | The check and **Say what would change**. Nothing is changed. |
| `gh workflow run game-center.yml -f apply=true`, on `develop` | As a push: the changes are made when the owner approves. |

What the script does with a difference:

- **A leaderboard or an achievement that is missing** is made, with its first version, its words and its image.
- **Points, a reference name, a score format** and the like are changed on the component.
- **Words or an image** are kept in numbered versions. A version that has not been sent to Apple is changed in place. A
  version that is live cannot be changed, so the script starts the next one; the owner sends it for review with a version
  of the app. A version that is with Apple (waiting, in review, accepted, pending release) is left alone, and the run
  says so.
- **Something in App Store Connect that the file does not have** is named and left alone.

What it never does: delete or archive anything, change an identifier (Apple does not allow it), add a component to a
version of the app, submit one for review or release one. Those stay with the owner, in App Store Connect.

It follows version 2 of the Game Center part of the App Store Connect API; version 1 is deprecated. The tests
(`tools/tests/test_game_center_sync.py`) run it against a stand-in for App Store Connect, so they need no key and reach
nobody. **It has not yet been run against Apple.** Make the first run a dry run (`gh workflow run game-center.yml`) and
read its summary before approving anything. To try it on a computer with a key of your own:
`python3 tools/game-center/sync_app_store_connect.py` says what would change, and `--validate` checks the file without
a key.

## The Snake pages

The game's public pages (home, How to play, Features, FAQ, What's new, Privacy, Support, Accessibility, Press) are
Markdown files in `docs/` whose body is plain HTML, drawn by `tools/docs/theme/marketing.html`. They are served by
iainchesworthlabs.com under `/snake/`. They are script-free, because a preview blocks every script, and every link on them
is relative, so they work under a preview address. The developer pages in `docs/` (building, architecture, testing, this
page, releasing and the rest) are not published: `exclude_docs` in `mkdocs.yml` lists them. A new developer page goes in
that list. The look is the company site's shell:

- `tools/docs/site-kit/v1/` is a **pinned copy** of the company kit (`icl.css`, the header and footer, the palette checker).
  Nothing is fetched at build or run time. `tools/docs/site_shell_hook.py` lets the templates include the kit's header and
  footer, puts `data-app="snake"` on any page that lacks it and copies `icl.css` to `assets/icl.css` of the site. To move to
  a newer kit, copy the files of its `/assets/v1/` folder over the copy, with the same layout (the checksum file is
  `kit/SHA256SUMS.txt`, the version `kit/VERSION.txt`), run `check_site_kit.py` and note the version in the pull request.
- `docs/theme.css` is the Snake palette: eight colour roles, light and dark, in one file. Its colours are the game's own,
  sampled from the app icon (see the comment at the top of the file). To change a colour, change that file only.
- `docs/assets/snake.css` is the layout the shell does not have. It uses the shell's variables and holds no colour.
- `tools/ci/check_site_kit.py` fails when a file of the copy differs from the kit's `SHA256SUMS.txt`, or the palette fails
  the company's checker (`check-palette.mjs`, Node 20 or later: shape, and WCAG 2.2 AA contrast against the shell's neutrals).
- `tools/ci/check_site_pages.py` builds the site (`mkdocs build --strict`) and fails on a page that has no title, no canonical
  address, no `lang="en-AU"` or not one `h1`; a page that has a `<script>` (except JSON-LD), a `<style>` or `style=`, loads
  anything from another host, links to an email address or uses a picture that does not show the game as built
  (`STALE_PICTURES` in the check: the renders of the design mock); an `<img>` with no `alt` attribute (`alt=""` is for a
  picture that only decorates), with an alt that is a file name or shorter than 10 characters, with no `width` and
  `height` in pixels, or, for a PNG, with a width and height of another shape than the file's; a link to a file or an anchor that is not in the built
  site, or to `/snake/` instead of a relative link; and a built site that lacks `privacy/` or `support/` (a store listing will
  print them, and they never move) or one of the other pages.

Both run in the **Site** job of `ci.yml`, which `CI Status` waits for, and `tools/ci/precheck.py` runs them after the shared
precheck (the page check is skipped there when `mkdocs-material` is not installed:
`pip install -r requirements/docs.txt`). Their unit tests are `tools/tests/test_check_site.py`: the Site job runs them, and
CTest runs them as `tools.site_checks` with the label `unit` when Python 3 is found (the tests that run the palette checker
skip themselves without Node).

#### Pictures on the pages

Only real pictures go on the pages: the icon, and screenshots of the game itself. `docs/design/screenshots/` are renders of
the design mock and stay off (`STALE_PICTURES`). The screenshots are `docs/assets/img/snake-*.png`: the title, game and
leaderboard screens in the computer layout (1100 by 700 pixels) and the phone layout (390 by 844 pixels). They were taken from
a Windows build of the game, by rendering `Main.qml` off screen with the software renderer, as `apps/snake/tests/test_screens.cpp`
does, with the game's own font (the TrueType files of `apps/snake/fonts/`; the prebuilt Qt used cannot
read woff2) and made-up names on the leaderboard (MAYA, LEO, JUNO, KAI, POPPY, SAM). They were not taken on a phone or on Linux
or macOS, and the pages say "the phone layout", not "an iPhone". To take them again, do the same at the same sizes, **look at
each picture**, and update the alt text and the `width` and `height` in the markup (a unit test compares them with the files).
A frame of CSS around a picture (`.sk-window`, the shell's `.icl-device`) is fine; a frame around a screen nobody has looked at
is not, and neither is badge or logo artwork that was not supplied. A photo of the ESP32-P4 board and a screenshot from a real
iPhone or iPad are the owner's to supply: the places for them are `OWNER DECISION` comments in `docs/index.md` and `docs/press.md`.

A change to a page needs no changelog fragment: the pages are under `docs/` and `tools/`. A change to the game does.

### Publishing the site

`docs.yml` builds the site on every pull request that touches `docs/**`, `mkdocs.yml`, `requirements/docs.txt`,
`tools/docs/**` or the workflow, and publishes it, as Field Test does, with the shared `docs-deploy-aws.yml`:

| Event | What happens |
|---|---|
| Pull request from this repository | The site is built and uploaded to `https://iainchesworthlabs.com/_preview/snake/pr-<n>/` through the preview role (`_preview/snake/*` only). One comment on the pull request carries the address. A preview needs the secrets below; without them the step says so and is skipped. |
| Pull request closed | The preview is deleted. |
| Push to `develop`, or a manual run from `develop` | The site is synced to `snake/` of the bucket with `--delete` and the path is invalidated, through the production role (`snake/*` only). It refuses any other ref. |

It replaces the earlier publication to GitHub Pages (`https://iainchesworthlabs.github.io/snake/`), which went out on a push
to `main`. The setup is the owner's, once:

1. In `icl-site`, add an entry to `APPS` in `infra/lib/apps.ts` (slug `snake`, repo `iainchesworthlabs/snake`, its numeric
   repository id, a role name such as `icl-site-deploy-snake`), customise this repository's OIDC subject claim as the other
   apps' are, and run the site stack's `cdk deploy`. The roles trust a push to `develop` and a pull request of this repository.
2. Set the repository secrets `DOCS_ROLE_ARN`, `DOCS_PREVIEW_ROLE_ARN`, `DOCS_BUCKET` and `DOCS_DISTRIBUTION_ID` from the stack's
   outputs.
3. Run the Docs workflow from `develop`. If the production deploy fails, the job *Explain a failed deploy* says what to check.
4. Point the repository's homepage (and `homepage` and `pages` in `.github/repo-settings.json`) at
   `https://iainchesworthlabs.com/snake/` and turn Pages off.

## Required checks

Two jobs defined in `ci.yml` are the required checks: **CI Status** and **Scan dependency diff**.
CI Status (which includes the Site job) fails when any job failed or was cancelled, and when a job the plan asked for was
skipped. Do not require checks from the called workflows by name; their names carry the caller's
job name as a prefix.

## Run the checks locally

```bash
python3 tools/ci/precheck.py
```

It needs a checkout of `icl-common-infra` next to this repository or in `ICL_COMMON_INFRA`.
