<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Game Center components

`components.json` is the record of what Game Center has in App Store Connect: the four leaderboards and the twelve
achievements, each with its identifier, reference name, points, whether it is hidden, whether it can be earned more
than once, and in English its name, its description before it is earned and its description after.
`tools/game-center/sync_app_store_connect.py` reads it and makes App Store Connect the same, so nothing is entered
there by hand. The game never reads this file; a test checks that the identifiers in the code match it, and that the
points are within Apple's limits.

- **Images.** Each achievement also needs an image, 1024 by 1024 pixels. They are in `images/`, one for each
  achievement, named for the last part of its identifier (`wrap.png` for `...achievement.wrap`).
  `tools/game-center/make_images.py` draws them: to change a picture, change the script and run it again, because a test
  compares the pictures here with what the script draws.
- **Identifiers.** An identifier can never be changed once it is entered in App Store Connect. Change one here only
  before it has been entered there, and change it in `apps/snake/src/viewmodel/game_center_director.cpp` with it.
- **Keeping App Store Connect the same.** Change this file or an image, and the `Game Center` workflow does the rest
  once the change is on `develop`: it says in the summary of its run what differs, and makes the changes when the owner
  approves them. `gh workflow run game-center.yml` says what differs at any time and changes nothing. The script makes
  what is missing and brings back what differs. It never deletes, never changes an identifier, and never adds a
  component to a version of the app or sends one for review. [Continuous integration](../../../../../docs/ci.md) has
  the rest.
- **Points.** Apple allows at most 100 points for each achievement and 1,000 in all. This set uses 300.
- **Submission.** The first achievements for an app go in the same submission as an app version. Ones added later can be
  submitted on their own.
- **Other languages.** Only English is here. The other languages are entered in App Store Connect and are not kept in
  this repository yet.
- **Leaderboards.** Each has its identifier, its reference name, its score format (`scoreFormat`), its sort order
  (`sort`: `DESC` puts the highest score first), which score counts (`submission`: `BEST_SCORE` or
  `MOST_RECENT_SCORE`) and its name in English. The words are Apple's own, from the App Store Connect API.

## Entering them in App Store Connect

The steps are in [`docs/app-store.md`](../../../../../docs/app-store.md), under "Game Center (in 1.0.0)", with a table
of the twelve achievements, their identifiers, points and images. That page also has the store text and the notes for
App Review that mention the achievements, and the rows of the TestFlight plan that test them.
