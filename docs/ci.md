# Continuous integration

The workflows in `.github/workflows/` call reusable workflows in `icl-common-infra`, pinned by
commit SHA. Dependabot proposes the SHA bump; the pull request also needs
`tools/ci/sync_cmake.py` run, because the CMake modules are vendored.

## What runs

| Event | Jobs |
|---|---|
| Draft pull request | Policy, Plan, workflow analysis, dependency scan |
| Ready pull request | Above, plus Linux release and sanitizer builds and coverage |
| Merge queue, push to `develop` | Above, plus Linux debug and TSan, macOS, Windows release and ASan |
| Nightly | CodeQL |
| Tag `v*` | Release |

The Plan job reads `.github/ci/plan.json`. A change to documentation only builds nothing. A path
the plan does not recognise builds everything.

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
- **iOS** (macOS runner): Qt's iOS kit and the macOS host kit, the `ios-release` preset through Xcode, unsigned, and
  `verify_app.sh` reads the bundle.

What they have **not** shown, and a device has to: that the program starts, that it draws at the size of the screen, that
touch and tilt work, that the sound plays and the audio session behaves, and that a package installs. Nothing runs the
program here: there is no emulator job, because a hosted emulator needs hardware acceleration and an image download that
is not reliable enough to stand behind a green tick.

Signing is the owner's. These two workflows use no secret.

**The signed iPhone and iPad build** is the `ios` job of `release.yml`, and it is the only place a secret is used for iOS. It
is off until the owner sets a repository variable: `SIGN_IOS_UPLOAD=true` for **TestFlight**, or `SIGN_IOS_ADHOC=true` for an
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

## Required checks

Two jobs defined in `ci.yml` are the required checks: **CI Status** and **Scan dependency diff**.
CI Status fails when any job failed or was cancelled, and when a job the plan asked for was
skipped. Do not require checks from the called workflows by name; their names carry the caller's
job name as a prefix.

## Run the checks locally

```bash
python3 tools/ci/precheck.py
```

It needs a checkout of `icl-common-infra` next to this repository or in `ICL_COMMON_INFRA`.
