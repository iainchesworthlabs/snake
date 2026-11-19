---
template: marketing.html
title: Privacy
title_tag: "Privacy: Snake"
description: "Snake collects no data about you. What the game keeps on your device, what permissions it asks for, and what leaves your device, which is only your scores to Game Center on an iPhone or iPad, if you are signed in."
layout: prose
---
# Privacy

Last changed: 19 November 2026.

**Snake collects no data about you.** It has no accounts, no analytics and no advertising, and the game does not use the network, except that on an iPhone or iPad, if you are signed in to Game Center, the game sends the score of each finished game to Game Center, which is Apple's. This page says what that means in practice.

<div class="icl-callout"><p><strong>In one minute</strong>
Snake collects no data about you. It has no account, no analytics and no advertising, and the game does not use the network. Your settings, your scores and the names you typed for the leaderboard are kept in one small file on your device. The only thing that can leave your device is your score, to Game Center, on an iPhone or iPad where you are signed in to Game Center.</p>
<p><strong>Check it yourself</strong>
The source code is public under the GNU General Public License, version 3 or later.</p></div>

## What the game keeps

The game keeps one small file in its own folder on your device. It holds:

- **Your settings.** How you steer, how sensitive tilt is, whether the axes are inverted, the sound and music switches, the volume and the reduced motion switch.
- **Which modes you have opened.**
- **Your achievements.** Which achievements you have earned, how many blueberries you have eaten in all, and which modes you have finished a game in.
- **Your leaderboards.** For each mode, the top 10: the name you typed, the score and the date the game ended. A name is letters A to Z and spaces, up to 12.
- **Your three most recent names**, so you can pick one again.

The file holds no device identifier, location or contacts, and no name other than the ones you type for the leaderboard. On the ESP32-P4 board the same things are kept in the board's own memory.

## What leaves your device

Nothing, with one exception. The game has no code of its own that connects to the internet. The exception:

- **Game Center (iPhone and iPad only).** If you are signed in to Game Center on the device, the game sends the score of each finished game, and the mode it was played in, to Apple's Game Center, where it goes on that mode's leaderboard under your Game Center player name. It sends nothing else. The game does not read your name or email, and the project receives nothing. If you are not signed in, nothing is sent, and the game asks Game Center, which may ask you to sign in. Apple's privacy policy covers what Game Center keeps. To stop, sign out of Game Center in the iOS Settings app. A score that cannot be sent, for lack of a connection, is not tried again. The leaderboards on the device are the same either way.

The About screen has a button that opens the game's source page in your browser. Your browser then connects to that page, as it does for any page you open, and the game sends it nothing.

## Permissions

- **iPhone and iPad:** the game asks for no permission. Game Center asks you to sign in itself, and the game sees only that you did. Tilt reads the motion sensor, which needs none. The readings steer the snake while you play. They are not kept.
- **Android:** the game asks for one permission, to vibrate. Android grants it when you install, and it gives the game no access to your data. The game turns off Android's app backup, so the file stays on the device.
- **Computers:** the game asks for no permission.

The game does not use your camera, microphone, location, photos or contacts.

## Deleting what the game keeps

- In Settings, Reset a leaderboard clears the scores of one mode. It asks you to confirm.
- Removing the game from a phone or tablet removes its file. On a computer, delete the game's data folder.
- Scores already sent to Game Center are Apple's to remove. Apple's Game Center settings (in the iOS Settings app, under Game Center) say how.

## Children

The game asks for no personal details. The name on the leaderboard is whatever letters the player types, and it stays on the device.

## Changes

If this page changes, the date at the top changes. Questions: see [Support](support.md).
