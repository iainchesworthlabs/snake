# Architecture

```text
libs/sim/             portable library (see below): board cells, directions, fixed-capacity containers,
                      seeded random numbers; public headers, sources, tests
  include/snake/sim/
  src/
  tests/
libs/input/           portable library: direction selector, key and gamepad maps; public headers, sources, tests
  include/snake/input/
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
The title screen's attract mode plays an ordinary game with a seed of its own, steered by `autoplay()`: a shortest path
to the food (a cell the tail is about to leave counts as free), refused when it would leave the snake less room than its
length, and otherwise the safe step with the most room. It keeps no state and draws no random numbers, so it never
touches the generator of a real game. Two autoplayed games are among the golden tests.
`libs/profile` is portable in the same way. It holds what a player keeps between runs: names, leaderboards, settings,
unlocks and the versioned bytes they are saved as. It never reads or writes a file. The platform service keeps the
bytes, so the desktop, the phones and the ESP32-P4 share the rules for names and ranking and each stores them its own
way. It is separate from `libs/core` because `libs/core` has a logger that uses iostream and a mutex.

The save file holds the settings, unlocks, recent names and leaderboards, at most 564 bytes. It starts with the marker
`SNKS` and a version number, which stay in those places in every version, then the size of the rest, the data, and an
FNV-1a checksum. `decode_save()` reads every version up to the current one and fills fields that a newer version added
with their defaults, so an update never loses a player's scores. It returns an error value, never a partly filled
save, for a file that is cut short, has bytes left over, fails its checksum or holds a value that is not allowed. A file
written by a newer program is reported as `NEWER_VERSION`, and the program then leaves it alone instead of replacing it
with an older save. The recorded games are separate files in the replay format, because they are up to 64 KB each and
change at other times. The platform service stores the bytes: it writes a new file next to the old one and renames it
into place, so a crash or a flat battery leaves the old save whole.
`libs/input` is portable in the same way, and may use floating point because none of it is part of the simulation. It
turns what the player does into directions for the input buffer in `libs/sim`; reading the sensor, the touch screen
and the keyboard stays with the platform, which hands the library plain numbers and its own key codes translated to the
library's `Key` and `GamepadButton` values. `DirectionSelector` is the rule that tilt and the left stick share: two
signed axis values become at most one direction. A direction is set when an axis reaches the engage value and the
larger axis wins, it is kept until its axis falls below the release value, and the axis in control keeps priority until
the other is a margin larger, so a diagonal hold does not flicker. Keys and buttons map through fixed tables: arrow
keys, W A S D and the d-pad steer, and the others ask for an action (pause and resume, menu, confirm).
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
