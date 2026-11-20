<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Game Center components

`components.json` is the source for what is entered in App Store Connect: the four leaderboard identifiers and the
twelve achievements, each with its identifier, reference name, points, whether it is hidden, whether it can be earned
more than once, and in English its name, its description before it is earned and its description after. The owner
enters it there by hand. The code never reads this file; a test checks that the identifiers in the code match it, and
that the points are within Apple's limits.

- **Images.** Each achievement also needs an image, 1024 by 1024 pixels. They do not exist yet.
- **Identifiers.** An identifier can never be changed once it is entered in App Store Connect. Change one here only
  before it has been entered there, and change it in `apps/snake/src/viewmodel/game_center_director.cpp` with it.
- **Points.** Apple allows at most 100 points for each achievement and 1,000 in all. This set uses 300.
- **Submission.** The first achievements for an app go in the same submission as an app version. Ones added later can be
  submitted on their own.
- **Other languages.** Only English is here. The other languages are entered in App Store Connect and are not kept in
  this repository yet.
- **Leaderboards.** Only the identifier and a reference name are here. The score format and sort order are set in App
  Store Connect.

## What to change in `docs/app-store.md`

That file is on the branch `docs/app-store-release`, not on this one. When the two branches meet, three places in it
need the achievements added:

1. The description's sentence "the only thing it ever sends is your score".
2. The notes for App Review.
3. The Game Center section and its rows of the test plan.
