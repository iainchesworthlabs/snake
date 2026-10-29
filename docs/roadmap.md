# Roadmap to version 1

This page records how the game described in the [product requirements](prd.md) is built, in what order, and the
decisions behind it. It is kept up to date as steps land. The visual design is in [`design/`](design/index.html).

## Where the project stands

- `libs/core`: logger, timing loop, a four-state `Game` that counts frames, window settings. No rules.
- `apps/snake`: a Qt Quick window with a view-model and a timer that calls `Game::tick`. English and Simplified
  Chinese only.
- CI and packaging cover Linux, macOS and Windows. There are no Android, iOS or ESP32 legs, and no audio, input,
  storage or settings code.

## Decisions

1. **Release shape.** Releases come in steps up to 1.0. Pre-releases per platform stage: `0.2.0` desktop (Windows,
   macOS, Linux), `0.3.0` Android and iOS, `0.4.0` ESP32-P4, then `1.0.0` when every acceptance item in the
   requirements is ticked on every platform. The first final release also creates `main` and starts the docs deploy.
2. **ESP32-P4 front end.** LVGL (MIT licence) with Espressif's `esp_lvgl_port`. Qt Quick does not run on the chip.
   Qt for MCUs lists the ESP32-P4 as a Tier 3 platform, but it is sold under a commercial licence and the project is
   GPL-3.0-or-later in a public repository, so it is not used.
3. **ESP32-P4 hardware.** The development board is the ESP32-P4X Function EV Board: chip revision v3.1 or later,
   32 MB PSRAM, 16 MB flash, a 7-inch 1024 x 600 MIPI DSI touch panel, an ES8311 audio codec with speaker output.
   It has no motion sensor and no user buttons. Tilt comes from an MPU-6050 on I2C, wired to header J1. Until the
   part is fitted the ESP work uses a fake sample source that replays recorded traces.
4. **Languages.** All eight in the standard: en, fr, es, de, he, yi, ja and zh_CN, with right-to-left tests.
5. **Name.** Snake is final.
6. **Art and audio.** Generated from the design and in code, with no outside artist for version 1.

## Gaps between the requirements and the standard

| Gap | Resolution |
|---|---|
| Android and iOS are in the standard (D8, section 10) but no project runs them in CI yet. | The legs are shared infrastructure, so they land in icl-common-infra and the template first. |
| ESP32-P4 is not in the standard's platform table, and ESP-IDF's stable toolchain (GCC 15.2 in v6.1) is below the GCC 16 floor. | Section 3.1 of the standard (portable libraries) and an ESP32 row in its platform table. The [spike](esp32-spike.md) built for the chip with GCC 16.1, which ESP-IDF `master` carries. |
| The requirements say no heap allocation per tick or per frame, which cannot hold for the Qt scene graph. | It applies to the simulation and input libraries. The render path is measured by frame time. |
| The standard wants an About dialog with licences and the source location. The requirements have no such screen, and Settings is not designed. | One more screen to design: Settings with About. |
| Pixelify Sans in the design is a Latin subset. | Fallback fonts for Hebrew, Yiddish, Japanese and Chinese (see Art, audio and fonts). |

## Architecture

```text
libs/sim/        portable library (standard 3.1): deterministic rules. No Qt, no iostream, no exceptions, no
                 clock, no allocation in the tick path. Integer arithmetic only.
libs/input/      portable library: tilt processor (calibrate, dead zone, threshold, hysteresis, low-pass),
                 swipe recogniser, key and gamepad maps, sample-source interface. The direction buffer is in sim.
libs/audio/      portable library: synthesised sound effects and music (voices, envelopes, note tables, sequencer).
libs/profile/    portable library: player names, leaderboard ranking, recent names, settings model, unlocks,
                 versioned persistence schema. It reads and writes bytes and never a file.
libs/core/       logger, timing loop, window settings (Qt-free, not portable: it uses iostream and a mutex).
apps/snake/      Qt Quick front end: theme from tokens, board item, screens, view-models, platform services
                 (storage, sensors, audio output, haptics, lifecycle) under src/platform/<os>/.
apps/snake-esp/  ESP-IDF project: panel, touch and MPU-6050 drivers, renderer on LVGL. Reuses sim, input, audio.
```

### Simulation

- Fixed-capacity types only. The snake is a ring buffer of 400 cell indices, occupancy is a 400-bit set, and the input
  buffer has two slots. Nothing allocates after construction.
- Food placement draws k from `[0, empty_count)` and takes the k-th empty cell by scanning the occupancy set, which is
  uniform and needs no list.
- The random generator is the project's own (PCG32 or xoshiro). The standard library's distributions are not portable
  across implementations, so they are not used. A golden test pins the first values for a seed.
- `tick(input)` is the only mutating call. Time is a tick count. The tick interval is a pure function of the foods
  eaten and the mode, returned to the caller, and the platform loop does the waiting.
- The four mutator layers (board, food, snake, time) are compile-time policies plus one runtime mode tag. Version 1
  uses the board layer only (wrap or wall). Other layers arrive with the modes that need them.
- History is an input log (seed, mode, one direction or none per tick) plus a snapshot every N ticks. Version 1 records
  the last game and the best game for each mode. Rewind and ghosts read the same log later.
- A 64-bit state hash (FNV-1a over the cells, direction, score and tick) is the golden value.

### Desktop and mobile front end (Qt Quick)

- A QML theme singleton is filled from `design/tokens.json` by a CMake step, with a test that fails when they differ.
- The board is one custom `QQuickItem` that builds scene-graph geometry (not 400 QML items) and draws the previous and
  current tick with an interpolation factor from the frame time. Integer scale, letterboxed.
- Screens use a `StackView` with the states of the flow diagram. View-models stay thin.
- Attract mode uses a small deterministic autoplayer in `libs/sim` (breadth-first search to the food, then any safe
  move) with its own seed, so it never touches the generator of a real game.

### Art, audio and fonts

The mock-ups draw everything from rectangles, rounded corners, inset shadows and a few small SVG paths, with the
numbers in the markup and in `tokens.json`. The art is therefore code, not image files.

- The board, snake segments (stripes, bottom bevel), head (eyes, pupils, tongue), blueberry with leaf and shine,
  particles, buttons, panels, medal tiles and the tilt plate are drawn from the token values at an integer scale. In
  Qt Quick that is scene-graph geometry. On LVGL it is the draw primitives. Icons (pause, lock, tick, sliders, medals)
  are the mock-up SVG paths in one shared table.
- A test compares a rendered board with the screenshots within a small tolerance, so drift from the design is caught.
- Sound effects and music are synthesised in `libs/audio` from square, triangle and noise voices with envelopes, with
  tunes as data and no sample files. The platform renders them to audio. The first tunes are placeholders to be
  judged by ear.
- Fonts: Pixelify Sans for Latin (it covers the accents French, Spanish and German need), Noto Sans Hebrew for Hebrew
  and Yiddish, Noto Sans JP, and Noto Sans SC, all under the SIL Open Font License. Game text is short, so a build step
  subsets each font to the glyphs in the translation files (fonttools `pyftsubset`). On the ESP the same glyph set is
  rendered to bitmaps. The exact families, weights and size cost are measured and recorded when the step lands
  (standard 7.4).

### ESP32-P4

- The ESP front end gets orientation from a sample-source interface in `libs/input`. The MPU-6050 driver sits behind it,
  and a fake source replays recorded traces, so the tilt processor and the ESP application are developed without the
  sensor. Tilt needs only the accelerometer (the direction of gravity). The driver, bus speed, address and pins are
  looked up when the part is fitted.
- Menus, pause and name entry work by touch, as the requirements say.
- Espressif's guide gives no minimum ESP-IDF version and names no touch controller for the panel. Both are looked up
  before the ESP work starts, as is the LVGL 9 status of `esp_lvgl_port` at an ESP-IDF version that supports chip
  revision v3.1.
- The stable ESP-IDF (v6.1) builds with GCC 15.2; `master` (v6.2 in development) with GCC 16.1. The spike
  ([esp32-spike.md](esp32-spike.md)) answered the questions: the four portable libraries build with all the project's
  warnings on GCC 14.2, 15.2 and 16.1 and play the same games on RISC-V 32 as on a PC, so they need no narrowing;
  a firmware with them, the board support package, LVGL 9.6 and `esp_lvgl_port` builds on `master` with GCC 16.1, but
  not on v6.1 with GCC 16.1 swapped in. Code size is the same within half a percent; speed needs the chip.

## Phases

Each pull request is one logical step and passes the PR gate. Those that touch threads, platform code or CMake run the
full matrix first (`gh workflow run ci.yml --ref <branch>`). Size: S under a day of review, M a few days, L more.

**Phase 0: housekeeping**

- Requirements and design merged (snake pull request 15). This page added.
- Shared pieces named in standard section 3.1: `icl_add_portable_library()` (reduced profile flags, configure-time
  dependency check), the include check with a per-library allowlist, and the ESP-IDF build workflow. Each is a pull
  request in icl-common-infra and needs a tag. Until they exist `libs/sim`, `libs/input` and `libs/audio` apply the
  rules by hand. (M)

**Phase 1: simulation core** (release 0.2.0 needs it)

1. `libs/sim` skeleton: cell and direction types, fixed ring buffer, occupancy set, random generator with pinned
   output, CMake target built with `-fno-exceptions -fno-rtti`, unit tests. (M)
2. Rules, one pull request each, with the tests the requirements name: tick order, edges (wrap and wall),
   self-collision with the vacating tail, eating and growth, win by full board. (M, three pull requests)
3. Input buffer: depth 2, each input checked against the direction after the one before it, duplicates and overflow
   dropped. (S)
4. Speed curve, scoring, combo, bonus food, game states. (M)
5. Input log, snapshots, state hash, golden replay tests, replay file format. (M)
6. Leaderboard and name rules, settings model, versioned persistence schema, in the new portable `libs/profile`
   (three pull requests: names and leaderboard, settings and unlocks, the save format). It is not in `libs/core`,
   because the ESP32-P4 needs the same rules and `libs/core` carries the iostream logger. (M)
7. Autoplayer for attract mode. (S)

**Phase 2: input library**

8. Tilt processor tested with synthetic accelerometer traces (calibration, 12 degree dead zone, 18 degree threshold,
   5 degree hysteresis, low-pass filter, larger axis wins), swipe recogniser, key and gamepad maps, sample-source
   interface with a fake source. (M)

**Phase 3: desktop application** (release 0.2.0)

9. Replace `Game` and the view-model with the simulation, a fixed-timestep loop and a minimal board item (plain
   rectangles). Playable with the keyboard. Existing tests migrate. (L)
10. Theme from tokens, bundled Pixelify Sans, board art (stripes, head, tongue, food bob), interpolation. (L)
11. Screens: title and mode select with locked modes, pause, game over, with a UI test for each. (L)
12. Name entry, leaderboard, persistence on disk through a platform service. (M)
13. Settings and About (needs the design), reduced motion, sound toggles, calibration entry. (M)
14. Juice and audio: particles, shake, death sequence, synthesised sounds and music. (L)
15. All eight languages: every string translated, right-to-left layouts and tests, fallback fonts. (L)
16. Release candidate: dry-run packaging, a playtest of Easy with a six-year-old, tuning of the defaults. (M)

**Phase 4: mobile** (release 0.3.0)

17. Android and iOS presets and CI legs in the template and icl-common-infra, with a Qt kit install. Signed output
    needs the owner's accounts and secrets (D8, D16). Builds and simulator tests do not. (L)
18. Platform services: sensors, orientation lock, lifecycle auto-pause, haptics, storage location. (M)
19. Tilt on device with the on-screen indicator, swipe and d-pad. Tuning on real hardware. (M)
19a. Signed iOS build for the owner's devices: shared `import-ios-signing` and `export-ios-ipa` actions in
    icl-common-infra, an ad-hoc IPA from the release workflow, and an install on an iPhone and then an iPad. Needs the
    owner's Apple certificate, profile and registered devices. Done before the ESP32 phase so that step 19's tuning can
    start. (M)

**Phase 5: ESP32-P4** (release 0.4.0)

20. Spike: build for the ESP32-P4 with GCC 16 (questions under ESP32-P4 above). A short note with the commands. (M)
    Done: [esp32-spike.md](esp32-spike.md). GCC 16.1 works on ESP-IDF `master`, not on v6.1; the libraries need no
    narrowing. Steps 21 to 23 follow its recommendations.
21. Amend the standard if needed. ESP-IDF project builds in CI from Espressif's container, with no hardware in CI. (M)
    Begun: the toolchains are pinned (`esp32p4` in `.github/toolchain-versions.json`) and `esp32-libraries.yml` builds the
    four portable libraries with GCC 15.2 and 16.1 ([ci.md](ci.md)). The standard's amendment is a pull request to
    icl-common-infra that the owner decides; the firmware build in Espressif's container comes with step 22's project.
22. Drivers (panel, touch, MPU-6050 once fitted), LVGL renderer (flat fills, hard shadows, capped particles), bitmap
    fonts, tilt navigation in menus. (L)
    Begun: `libs/raster` paints the board (tested on the desktop) and `apps/snake-esp` is an ESP-IDF project with a title,
    the game, the Shed button and the end of a game on the board support package's panel and touch, built in CI with ESP-IDF
    v6.1 (`esp32-firmware.yml`). Not done: the MPU-6050 and the tilt, sound through the ES8311, the pixel fonts and the
    translations, the saved profile, hard shadows and particles. Nothing has run on a board.
23. On-device frame-rate and tilt-feel checks, recorded here. (M)

**Phase 6: 1.0**

24. Tick the acceptance list on each platform, assemble the changelog, tag `v1.0.0`.

## Tests and gates

- A unit test for every rule in the requirements, named after the rule. Coverage stays at 90 percent statement and
  80 percent branch (D6), with UI tests counted.
- Golden replays: recorded games with their hashes run in the unit suite on every platform leg. The hash is the proof of
  platform independence the requirements ask for.
- A property test: random inputs for a million ticks never break an invariant (head on the board, no overlap, food on
  an empty cell, length equals 3 plus the foods eaten), run under ASan, UBSan and TSan.
- Tilt: recorded traces with expected direction sequences. The feel is judged on a device.
- Frame time: a `perf` test on the board item with a budget, and direction-change latency measured in an instrumented
  build.
- The portable libraries are built with the ESP-IDF toolchain once phase 5 starts.

## Rules the requirements leave open, and the defaults used

1. Bonus food scores 50 flat, not multiplied by the combo.
2. Bonus food restarts the combo window without raising the combo (a lapsed window drops it to x1 first), steps the
   speed like normal food, and does not count towards the fifth.
3. The bonus item goes on a uniformly random empty cell other than the food's, and is skipped if fewer than two cells
   are empty.
4. The combo window is 20 ticks after the previous eating tick, so a second food on tick 20 still counts.
5. If a bonus item times out on the tick the snake would eat it, eating wins.
6. A new game starts with an empty input buffer, and inputs during a pause are dropped.
7. Unlock scores unlock a mode for good, stored with the settings.
8. Easy ends in a perfect game when the board fills, like Classic.
9. The best score on the gameplay screen is the top entry of the current mode's leaderboard.
10. The tick interval is in whole milliseconds and never below the floor, as in the requirements table.
11. A new snake has its head at column 10, row 10 (the 20 by 20 board has no single middle cell) with the body trailing
    to the left, and the first food is placed like every later one.
12. A tick that ends the game leaves the snake where it was: it does not move into the wall or the body.
13. A bonus item still on the board when the next one is due is replaced by the new one. An item takes a tick off its
    40 on every tick the snake does not eat it, so it can be eaten on the 40th tick after it appears and is gone after
    that.
14. Food never goes on the bonus item. A bonus item on the last empty cell gives way to the food.
15. A name is saved without the spaces at its start and end, so "  ann " and "ANN" are one name. The same name may
    appear on a leaderboard more than once.
16. An entry's date is the end of the game in UTC seconds, given by the platform. It is shown, not used to rank.
17. A score equal to the 10th entry of a full board does not get on, because the earlier entry ranks higher.
18. When two axes tie exactly, the horizontal one wins. The left stick sets a direction at 60 percent of full push,
    lets go below 40 percent and changes axis when the other is 15 points larger. Tilt uses the same rule in degrees.
19. On a gamepad Start pauses and resumes, Select and the east button open the menu, and the south button confirms. A
    d-pad press and the left stick both steer. Face buttons for later modes are added with those modes.
20. Tilt samples are in g with the device lying face up reading z = 1. The filter's time constant is 50 ms, so a
    steady 25 degree tilt sets a direction after about 70 ms at 100 samples a second. A sample outside 0.5 to 1.5 g is
    skipped. These are starting values for tuning on a device.
21. A swipe is given when the finger has moved 24 device-independent pixels along its larger axis, at once and not on
    lift, and a finger that carries on is measured from the point of the last swipe. Only the first finger is followed.
22. Portals has walls at the edges and four portals, two pairs, put on empty cells with the game's generator when the
    game starts: not in the row the snake starts in, and no two next to each other. A head that steps onto a portal
    lands on the other portal of its pair in the same tick, and goes on in the direction it had. The cell it lands on
    must be free (the tail's cell counts as free, as for any step) or the game ends. Stepping onto a portal never
    collides, even when the body lies on it, because the head does not stay there. Food, the bonus item and the bomb
    food are never put on a portal. The speed is Classic's.
23. Hot potato has walls. After every third normal food, once the snake has at least 6 parts and no bomb is on the board
    or in the snake, a bomb food appears on an empty cell and stays 50 ticks. Eating it scores 100 flat, restarts the
    combo window without raising the combo, steps the speed, grows the snake and does not count towards the fifth. The
    bomb then sits in the part of the body that ate it and does not move: the body moves past it, so it is one part
    nearer the tail with every tick. When it is on the tail and the tail is about to move away the bomb goes off and the
    game ends. A tick that eats keeps the tail where it is, so it delays the bomb by one tick. Shedding (Enter, the south
    button of a gamepad or the Shed button on the screen) drops the bomb and every part behind it at once and frees their cells; it does
    nothing while the bomb is still in the head, because that would drop the whole snake. Shedding late keeps more of
    the snake. The speed starts at 160 ms and gets 3 ms shorter for each food, down to 80 ms.
24. The save file has a leaderboard for each of the four modes (version 3; a version 1 or 2 file reads with the two new
    boards empty), and a replay holds a shed as an input of its own (the byte 4 where a turn has 0 to 3).

## Risks

- Mobile and ESP carry most of the uncertainty, and neither is exercised by CI today. They come after a playable desktop
  game, so the rules and the feel are settled first.
- Tilt tuning needs real devices and a six-year-old, so it is planned as playtest rounds as well as tests.
- Qt Quick frame pacing on low-end Android devices is unknown. The perf budget shows it early.
- Gamepad and haptics support in Qt 6.12 is looked up before the settings step (13) and the mobile services step (18).
- The ESP dev board has no motion sensor, so tilt feel on the ESP cannot be judged until the MPU-6050 is fitted.
