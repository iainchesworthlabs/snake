<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Game Center components

`components.json` is the source for what is entered in App Store Connect: the four leaderboard identifiers and the
twelve achievements, each with its identifier, reference name, points, whether it is hidden, whether it can be earned
more than once, and in English its name, its description before it is earned and its description after. The owner
enters it there by hand. The code never reads this file; a test checks that the identifiers in the code match it, and
that the points are within Apple's limits.

- **Images.** Each achievement also needs an image, 1024 by 1024 pixels. They are in `images/`, one for each
  achievement, named for the last part of its identifier (`wrap.png` for `...achievement.wrap`).
  `tools/game-center/make_images.py` draws them: to change a picture, change the script and run it again, because a test
  compares the pictures here with what the script draws.
- **Identifiers.** An identifier can never be changed once it is entered in App Store Connect. Change one here only
  before it has been entered there, and change it in `apps/snake/src/viewmodel/game_center_director.cpp` with it.
- **Points.** Apple allows at most 100 points for each achievement and 1,000 in all. This set uses 300.
- **Submission.** The first achievements for an app go in the same submission as an app version. Ones added later can be
  submitted on their own.
- **Other languages.** Only English is here. The other languages are entered in App Store Connect and are not kept in
  this repository yet.
- **Leaderboards.** Only the identifier and a reference name are here. The score format and sort order are set in App
  Store Connect.

## Entering them in App Store Connect

The steps are in [`docs/app-store.md`](../../../../../docs/app-store.md), under "Game Center (in 1.0.0)", with a table
of the twelve achievements, their identifiers, points and images. That page also has the store text and the notes for
App Review that mention the achievements, and the rows of the TestFlight plan that test them.
