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
  chained to Qt's. The `android-arm64-debuggable` preset makes a debug-signed APK, and `verify_apk.sh` reads it: the
  application id, the version, the platform levels, **no permission**, `arm64-v8a` only, the library, the icon and the
  signature. The APK is uploaded for 14 days.
- **iOS** (macOS runner): Qt's iOS kit and the macOS host kit, the `ios-release` preset through Xcode, unsigned, and
  `verify_app.sh` reads the bundle.

What they have **not** shown, and a device has to: that the program starts, that it draws at the size of the screen, that
touch and tilt work, that the sound plays and the audio session behaves, and that a package installs. Nothing runs the
program here: there is no emulator job, because a hosted emulator needs hardware acceleration and an image download that
is not reliable enough to stand behind a green tick.

Signing is the owner's. These workflows use no secret. The Android upload key and the Apple account, certificate and
provisioning profile are not in the repository, and a release signed with them is a later step.

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
