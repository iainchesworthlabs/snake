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

`ProfileViewModel` is what QML sees of the player's record. It watches a `GameViewModel` and, when a game ends, notes
the score against the best of the game's mode and against the unlock rules of `libs/profile`, which gives the game-over
screen its figures (`newBest`, `pointsToBest`, `unlockedNow`). It owns the `profile::SaveData` in memory and a
`ModeListModel`, the four modes of the title screen with their names, what each one says under its name, and whether it
is locked, open or only open on paper: Portals and Hot potato can be unlocked but are not in the game yet, so they are
never startable. The best score of a mode is the top of its leaderboard.

A game that makes the top 10 (`Leaderboard::rank_for()`) does not go on the board at once: it waits as the pending score
(`qualifies`, `pendingScore`, `pendingRank`) while the player types a name. `typeCharacter()`, `eraseCharacter()` and
`chooseRecent()` build a `profile::PlayerName`, `standing` gives the lines around the place the score will take, and
`saveScore()` puts the entry on the board under the name without its outer spaces, with the time the game ended (a
clock the tests can replace), and remembers the name. A game that is not saved is lost when the next one ends or the
program closes, and does not count as the best to beat.

`SaveFile` (`src/platform/`) is the platform service that keeps `SaveData` in one file. The profile library turns the
data into bytes; `SaveFile` reads them, and writes them through `QSaveFile`, so a crash leaves the old file or the new
one and never half of either. `ProfileViewModel::saveFile` names the file (`save.snk` in the user's data folder; the
`--save-file` option of the program changes it; empty keeps everything in memory, which is what the tests use). Setting
it loads the file. A missing file is a first run. A file that fails its checks is renamed `save.snk.damaged` and the
profile starts afresh. A file from a newer version, or one that cannot be read, is left alone and the profile stops
writing, so an older program never destroys what a newer one saved. The profile is written after each game that ends
(the unlocks may have changed) and after each score is saved; a write that fails is logged and tried again next time.

The leaderboard is a `LeaderboardModel` (place, name, score as six digits, and whether the line is the one saved last) of
the mode in `boardMode`; saving a score sets the mode and marks its line, and loading a save file clears the mark.

The screens follow the state of the game, so there is almost no navigation to keep in step with it. The two screens
that are not states of the game, name entry and the leaderboard, are an `overlay` property of `Main.qml` (`""`,
`"nameEntry"` or `"leaderboard"`) shown over the title or the game over screen; a game that starts clears it. The game
over screen offers Save score in place of Play again while a score waits for a name, and Menu lets that score go. The
leaderboard's Back goes to the title (through `newGame()` when it was reached from the end of a game). `NameEntryScreen`
keeps the keyboard on the screen itself, so letters and Space type into the name and the buttons on it answer touch only;
steering the letter grid with tilt or a gamepad comes with the settings and the tilt work. `Main.qml` shows the title
screen while the game has not started, the game screen (the score, the best score and the board) while it runs or is
paused, the pause dialog over the game while it is paused, and the game over screen after it. Each screen takes the
keyboard when it shows and puts it on its main button. The three screens are drawn on the 390 by 844 page of the design
(`PhonePage`), scaled to fit the window and centred, so the phone layout is used at any window size; the design has no
desktop title, pause or game over screen yet. The buttons (`ToyButton`) and the mode cards (`ModeCard`) are drawn from
rectangles in the style of the design, with a hard shadow, an outline and a focus ring. The up and down arrows move
between them along the Tab order, so a locked mode is skipped, and a mode takes the choice when it gets the focus. The
QML files sit at the root of the `SnakeApp` module, which is where `qmllint` looks for the types of the other files.

`board_scene` works out what the board looks like and `BoardItem` hands it to the scene graph. The scene builder is
plain C++ over the simulation's state: `layout_for()` picks the largest whole-pixel cell that fits the item, with the
frame (a quarter of a cell) and its hard shadow (twice that), centred in the item; `background_shapes()` gives the
shadow, the frame and the checkerboard; `game_shapes()` gives the striped snake, the head with its eyes and tongue,
the food with its highlight and leaf, and the bonus item. Everything is rectangles on whole pixels, in the measures
and colours of the design (rounded corners and the berries are built from rows of rectangles), so that every renderer
draws it the same and the edges stay crisp. The shapes are tested as data, without a window.

The snake glides between cells. The view-model gives `tickProgress()`, the time since the last tick as a fraction of
the interval, and the scene draws each piece of the snake that far into its next move: the head into the cell it will
take, facing the way it will turn, and every other piece into the cell of the piece in front of it. The stripes
alternate by place in the snake, so they travel with it. A move that ends the game, that crosses the edge of a
wrapping board or that is not coming (the game is paused or over) is not drawn ahead. When the next cell has food, the
tail stays and a new piece is uncovered behind it, so the body stretches. At the end of a tick the picture is the one
the next tick starts from, so nothing jumps. This draws the game a fraction of a tick ahead of the simulation, which
adds no delay to the controls. Pausing keeps the picture where it is and resuming carries on from there. The board
item redraws on `progressChanged`, which the view-model emits each frame that the game runs.

`BoardItem` turns the shapes into the window's rectangle nodes (`QQuickWindow::createRectangleNode()`) and not its own
geometry, because the software renderer, which the UI tests and CI use, draws only the standard node types and shows
nothing for a custom one. The background is drawn again only when the layout changes; the game's shapes are drawn
again when the board changes or the food moves. The `foodLift` property, from 0 to 1, raises the food and the bonus
item; `Main.qml` animates it up and down (0.9 seconds a cycle, ease in and out) so the food bobs. The tests draw the
item into an off-screen window and read the pixels back.

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
