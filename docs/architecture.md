# Architecture

```text
libs/sim/             portable library (see below): board cells, directions, fixed-capacity containers,
                      seeded random numbers; public headers, sources, tests
  include/snake/sim/
  src/
  tests/
libs/input/           portable library: tilt processor, sample sources, swipe recogniser, direction selector, key and
                      gamepad maps; public headers, sources, tests
  include/snake/input/
  src/
  tests/
libs/profile/         portable library: player names, leaderboards, recent names; public headers, sources, tests
  include/snake/profile/
  src/
  tests/
libs/core/            Qt-free library: logging, version, window settings; public headers, sources, tests
  include/snake/
  src/
  tests/
apps/snake/           Qt Quick application
  src/                main.cpp
  src/input/          Qt key codes to the keys of libs/input
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
The game loop is a fixed timestep. The platform tells a `TickPacer` how much time has passed since the last frame, in
microseconds, and asks it for each tick that is due, passing the session's current tick interval. The pacer reads no
clock. It keeps at most 250 ms, so a stall (a dragged window, a slow frame) does not make the game race through the
ticks it missed, and the simulation is unaffected by frame timing: a game is the same whatever the frames were.
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

Tilt comes in through a `SampleSource`, an interface that returns accelerometer samples in g in the frame of the
screen (x to the right edge, y to the top edge, z out of the screen, a device lying face up reading z = 1). The phones
implement it over the system sensor API, the ESP32-P4 over the MPU-6050 driver, and tests and the ESP work before the
IMU is wired use a `TraceSource`, which plays back a recording. A platform whose sensor has other axes, signs or units
converts before it hands a sample over. `TiltProcessor` smooths the samples with a low-pass filter (50 ms time
constant), measures two angles from the filtered gravity (the roll about the long axis and the pitch above the
horizontal), subtracts the neutral pose set by `calibrate()`, and gives the angles to a `DirectionSelector` set to
12, 18 and 5 degrees. The player can hold the device at any angle from flat to nearly upright. The result is the
direction in force, not an event. The caller offers it to the input buffer every frame it is set, and the buffer drops
repeats. The reading also carries the two angles, which the on-screen tilt indicator draws. A sample whose size is not
close to 1 g (free fall, a knock, a failed read) is skipped. The library is built without RTTI, so no virtual function
of a class that its users also derive from or instantiate is defined in a source file.

`SwipeRecogniser` follows the first finger on the touch screen and gives a direction as soon as the finger has gone
24 device-independent pixels along its larger axis, without waiting for it to lift. A finger that carries on is measured
from where the last direction was given, so one stroke can turn the snake more than once. The platform passes touch
presses, moves and releases, and calls `cancel()` when the system takes the touch away. The on-screen d-pad is a set of
buttons that call the same turn function as every other input and needs no code in the library.
The [roadmap](roadmap.md) lists the libraries that follow.

## The application

`GameViewModel` is what QML sees of the game. It owns a `sim::Session` and a `sim::TickPacer` and holds no rules. A
`FrameAnimation` in `Main.qml` runs in step with the display and calls `advance()` with the time each frame took; the
view-model runs the ticks that are due, at the speed the game has reached, so the game plays the same whatever the
frame rate. Key presses go to `handleKey()`, which maps the Qt key code to the library's key and either steers or
performs an action: Enter starts a game, resumes a paused one and starts a new one after game over, Space pauses and
resumes, and Escape pauses until there is a menu. A held key's repeats are used up without effect, and a window that
loses the focus pauses the game. Each new game takes its seed from the system's random number generator, or from a
function the tests give. The state, score and mode are properties, and `boardChanged` tells the board item to redraw.

`BoardItem` draws the board from the view-model's session as plain rectangles: the board, the body, the head, the food
and the bonus item, in the largest square that fits the item. It uses the window's rectangle nodes
(`QQuickWindow::createRectangleNode()`) and not its own geometry, because the software renderer, which the UI tests
and CI use, draws only the standard node types and shows nothing for a custom one. The tests draw the item into an
off-screen window and read the pixels back. The colours are placeholders until the board art arrives.

The look of the application comes from the Toybox Pixel design tokens in `docs/design/tokens.json`. `tokens.hpp` holds
the colours and the font family as constants, and a test compares each with the JSON file, so a change to the file
fails the build until the constants follow. The `Theme` singleton gives QML the colours (`Theme.ink`,
`Theme.surface`) and the family name. The Pixelify Sans files from `docs/design/fonts` are compiled into the
executable and loaded at start-up, and the application font is that family with Courier New and the system monospace
face behind it. The files hold the Latin range only; the fonts for the other scripts come with the translations. The
font's licence file is installed with the program.

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
