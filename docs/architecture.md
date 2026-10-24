# Architecture

```text
libs/sim/             portable library (see below): board cells, directions, fixed-capacity containers,
                      seeded random numbers; public headers, sources, tests
  include/snake/sim/
  src/
  tests/
libs/profile/         portable library: player names, leaderboards, recent names; public headers, sources, tests
  include/snake/profile/
  src/
  tests/
libs/core/            Qt-free library: timing, logging, game state; public headers, sources, tests
  include/snake/
  src/
  tests/
apps/snake/           Qt Quick application
  src/                main.cpp
  src/viewmodel/      thin QObject classes that QML binds to (camelCase names, see below)
  qml/                Main.qml
  translations/       snake_<language>.ts
  tests/              view-model and translation tests
cmake/icl/            vendored CMake modules from icl-common-infra (do not edit)
tools/                project scripts, baselines and coverage floors
```

## Portable libraries

`libs/sim` holds the rules of the game and is a portable library (section 3.1 of the project standard). It includes
no Qt or platform header and uses no exceptions, RTTI, iostream or clock, and nothing in it allocates after
construction, so the same code runs on the desktop, on phones and on the ESP32-P4. Its sources are built with
exceptions and RTTI off. The random number generator is its own (PCG32), because the standard library's generators
and distributions give different numbers on different implementations, and a recorded game must replay identically.
A game can be saved and compared. A snapshot holds all of a game's state and has a byte form that is the same on every
platform. The state hash is FNV-1a over those bytes. A replay is the mode, the seed and every turn, with the tick count,
score and hash the game ended on, and it has its own byte form. The golden replay tests play recorded games and compare
the result with the stored values on every platform leg, which is the check that the rules behave identically
everywhere. The golden values come from the library itself, so a change to a rule makes them fail on purpose and they
are updated in the same pull request.
`libs/profile` is portable in the same way. It holds what a player keeps between runs: names, leaderboards, settings,
unlocks and the versioned bytes they are saved as. It never reads or writes a file. The platform service keeps the
bytes, so the desktop, the phones and the ESP32-P4 share the rules for names and ranking and each stores them its own
way. It is separate from `libs/core` because `libs/core` has a logger that uses iostream and a mutex.
The [roadmap](roadmap.md) lists the libraries that follow.

## Platform code

There is no platform-specific code yet. When there is, the differences live in `src/platform/<os>/` with
the same file names in each directory.
`icl_platform_sources(target)` adds the directory that matches the target system. Source files
contain no `#if` on the platform or compiler; the preprocessor check fails the pull request if
one appears. Other directives are counted per file against `tools/preprocessor-baseline.json`,
which can go down but not up.

## Naming

Namespaces, functions and variables use `snake_case`. Types use `PascalCase`. Members use
`m_PascalCase`. Constants and enumerators use `UPPER_SNAKE_CASE`. Braces follow Allman style.
`.clang-format` and `.clang-tidy` enforce this.

## View-models

QML sees the game through `snake::ui::GameViewModel`, which holds a `snake::Game` and a timer and has no
rules of its own. Names that QML sees (properties, signals, invokable functions) are camelCase; the
override is in `apps/snake/src/viewmodel/.clang-tidy`. A bulk change emits one signal, not one per item.

## Vendored modules

`cmake/icl/` is a copy of `icl-common-infra/cmake/`. `cmake/icl/SOURCE` records the commit.
CI compares the copy with the commit its workflows pin and fails on a difference. Update with
`python3 tools/ci/sync_cmake.py --infra <checkout of icl-common-infra>`.
