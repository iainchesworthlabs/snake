# snake

[![CI](https://github.com/iainchesworthlabs/snake/actions/workflows/ci.yml/badge.svg?branch=develop)](https://github.com/iainchesworthlabs/snake/actions/workflows/ci.yml)
[![CodeQL](https://github.com/iainchesworthlabs/snake/actions/workflows/codeql.yml/badge.svg?branch=develop)](https://github.com/iainchesworthlabs/snake/actions/workflows/codeql.yml)
[![Licence: GPL-3.0-or-later](https://img.shields.io/badge/licence-GPL--3.0--or--later-blue)](LICENSE)

A Snake game on Qt 6 and C++23 for Linux, macOS and Windows. The game itself is not implemented yet: the window
opens, a 60 Hz tick counts frames, and the structure is ready for game logic. The project started as a clean port
of a 2017 Visual Studio prototype (see [docs/migration.md](docs/migration.md)); its history is in
[iainchesworth/snake.old](https://github.com/iainchesworth/snake.old).

## Layout

| Path | Contents |
|---|---|
| `libs/sim` | The rules of the game: a portable library with no Qt. Unit tested with Catch2. |
| `libs/input` | Tilt, swipe, key and gamepad input turned into directions: a portable library with no Qt. Unit tested with Catch2. |
| `libs/profile` | Names, leaderboards, settings and unlocks: a portable library with no Qt. Unit tested with Catch2. |
| `libs/core` | Qt-free logic: frame timing, logging, game state. Unit tested with Catch2. |
| `apps/snake` | The Qt Quick application, its view-model, translations (English and Simplified Chinese) and tests. |
| `docs/` | Developer documentation, built with MkDocs. |

## Build

The project builds with CMake presets, vcpkg and Qt 6.12. The build finds them through environment variables:

| Variable | Meaning |
|---|---|
| `ICL_VCPKG_STORE` | Directory for vcpkg checkouts of the organisation's vcpkg fork (one per baseline commit) |
| `ICL_VCPKG_MIRROR` | Optional clone of the fork to make them from |
| `ICL_QT_ROOT` | The Qt kit, for example `~/Qt/6.12.0/gcc_64` |

```bash
cmake --preset linux-release        # macos-release, windows-msvc-release on the other systems
cmake --build --preset linux-release
ctest --preset linux-release
```

Each platform's steps, the compiler floor (GCC 16, Clang 23, MSVC 14.51, newest Apple Clang) and the other
presets (sanitizers, coverage) are in [docs/building.md](docs/building.md).

## Releases

A release is a `v*` tag on a `develop` commit. The Release workflow builds the Linux, macOS and Windows packages,
and the publish step adds `SHA512SUMS`, detached GPG signatures, an SPDX SBOM and build provenance. To verify a
download, see [docs/releasing.md](docs/releasing.md); the public key is `docs/signing-key.asc`.

Licence: GPL-3.0-or-later. Contributions need a DCO sign-off (`git commit -s`). See [AGENTS.md](AGENTS.md) for the
working rules.
