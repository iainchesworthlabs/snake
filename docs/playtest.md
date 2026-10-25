# Playtest and tuning

This page is for the person who plays the first release candidate (0.2.0, the desktop game) and for whoever tunes it
after. The roadmap's step 16 has three things in it that need a person: **a playtest of Easy with a six-year-old**,
**listening to the sounds**, and **tuning the defaults** from what is seen. A program cannot do them. What it can do is
in the first section; the rest is a script and a list of what to write down.

## What a bot says

`sim::autoplay()` plays by the shortest way to the food, with no reaction time and no mistakes of the hands. It is not a
player. It shows what the rules allow, so that a target can be seen to be possible or not. These are 300 games of it
in each mode, at most 6,000 ticks each (the seeds 1 to 300; the numbers come from `libs/sim` as it is on `develop`):

| | Easy | Classic |
|---|---|---|
| Score, a poor game (10th percentile) | 4,870 | 1,890 |
| Score, the median game | 5,860 | 2,640 |
| Score, a good game (90th percentile) | 6,700 | 3,330 |
| Foods eaten in the median game | 150 | 85 |
| Seconds of play in the median game | 319 | 105 |
| Games that reach 2,000 (opens Portals) | 100% | 87% |
| Games that reach 3,000 (opens Hot potato) | 99% | 22% |
| How the game ended | 285 self, 15 reached the cap | 234 wall, 66 self |

The tick, in milliseconds, after some foods:

| Foods eaten | 0 | 5 | 10 | 15 | 20 | 30 | 40 | 60 |
|---|---|---|---|---|---|---|---|---|
| Easy | 180 | 170 | 160 | 150 | 140 | 120 | 120 | 120 |
| Classic | 150 | 135 | 120 | 105 | 90 | 70 | 70 | 70 |

What this suggests, to be checked against a person:

- **The unlock targets are an expert's.** A bot with a perfect eye gets 2,000 in Classic in 87 games of 100. A child who
  steers by tilt will not come near; a full combo is 50 points a food (10 times 5), so 2,000 is 40 foods eaten with every
  one inside the 20 tick window, and 3,000 is 60. If a six-year-old is to open Portals, 2,000 is far too high. The
  roadmap says "unlock scores are placeholders to tune in playtesting"; this is why.
- **Classic reaches its floor of 70 ms after 27 foods.** That is 14 cells a second, about 1.4 seconds to cross the
  board. A bot manages. Whether tilt can is a thing to feel on a device.
- **Easy ends at 120 ms**, 8 cells a second, after 30 foods. For a young child that may already be too fast at the start
  (180 ms is 5.5 cells a second). The walls wrap in Easy, so nothing ends a game but the snake meeting itself.

## The playtest of Easy with a six-year-old

Before: a build of the release candidate on the device the child will use (a phone or tablet for tilt, or the desktop
build with the keyboard), a clean save (`--save-file` with a new name, or delete `save.snk`), the sound on at a
volume that suits the room, and someone to write. Do not explain the game. Say "this is a snake game, you can play".

Write down, in the table below, for each game of the first ten minutes:

1. How long before the first food was eaten, and whether the child found how to steer without being told.
2. What ended each game (a wall cannot end Easy), and whether the child looked surprised, bored or upset.
3. Whether the child could read the title, the game over screen and the name entry, and whether the recent names
   helped. The name entry takes the letters A to Z, which a six-year-old may not know in order.
4. Whether they wanted to play again, and how soon, and whether they asked for the sound or music off.
5. Where they got stuck in the menus: the Settings button, Pause, and the leaderboard.

Then ask for a name to be typed once, and whether the tilt (or keys) felt too sensitive, too sluggish, or about right.

| Game | First food after | Score | Ended by | Wanted again? | Notes |
|---|---|---|---|---|---|
| 1 | | | | | |
| 2 | | | | | |
| 3 | | | | | |

## The defaults, and what to change

Each is a named constant. Change one in a pull request with a changelog fragment, and say in it what was seen.

| Default | Value now | Where | Look for |
|---|---|---|---|
| Easy speed | 180 ms, 2 ms a food, floor 120 ms | `speed_curve_of()` in `libs/sim/include/snake/sim/mode.hpp` | Too fast at the start or after ten foods |
| Classic speed | 150 ms, 3 ms a food, floor 70 ms | the same | Floor too fast to steer |
| Combo window | 20 ticks, up to x5 | `Scoring` in `scoring.hpp` | A child never sees the x2 |
| Bonus item | 50 points, every fifth food, 40 ticks | `Scoring` and `Simulation` | Never reached before it goes |
| Unlock targets | 2,000 and 3,000 in Classic | `unlock_rule()` in `libs/profile/include/snake/profile/unlocks.hpp` | Far above what the child scores |
| Tilt | 12 degrees to rest, 18 to turn, 5 to change axis, 50 ms filter | `tilt_processor.hpp` | Turns by itself, or does not turn |
| Swipe | 24 device-independent pixels | `swipe_recogniser.hpp` | Too short or too long a stroke |
| Name | 12 letters, A to Z and space | `PlayerName` in `libs/profile` | |
| Death | 0.6 s of flash, a pop every 0.05 s | `Effects` in `apps/snake/src/board/effects.hpp` | Too long to wait, though a key skips it |

Changing a speed or a score changes the golden replays and the hashes in `libs/sim/tests`; the tests say which, and the
numbers are to be regenerated by the library, not edited by hand.

## Listening to the sounds

The sounds and tunes are made in code (`libs/audio`) and have not been heard by anyone but the program that tests them.
Play each on the target device through its own speaker and with headphones, and write down what is wrong. Short
sounds can be changed in `libs/audio/src/tunes.cpp`: a note is a MIDI number, a segment is a slide from one note to
another over some milliseconds, at a volume from 0 to 255.

| Sound | Heard when | Too loud / harsh / long / unclear? | Change |
|---|---|---|---|
| Eat | the snake eats | | |
| Bonus | the bonus item is eaten | | |
| Crash | the game ends | | |
| Button | any button is pressed | | |
| Start | a game starts | | |
| New best | a score beats the best | | |
| Title tune | on the title | | |
| Game tune | during a game | | |

Things to listen for: the game tune under many eat sounds (the effects take half the level and the music a quarter, and
nothing ducks the music), the click at the start of any sound (there should be none), and the phone's speaker, which has
little bass, so the triangle voice of the tunes may disappear.

## The languages

All eight languages are machine translations that no speaker has read. Before a release, someone who reads each should
look at every screen once (`snake --language fr`, and so on) and say what is wrong: wrong words, a rude or odd one, text
that does not fit, a word that a child would not know. Yiddish and Hebrew need a reader most, because they are
mirrored right to left, and Yiddish spelling is the least certain.
