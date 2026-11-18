---
template: marketing.html
title: Privacy
title_tag: "Privacy: Snake"
description: "Snake collects no data about you. What the game keeps on your device, what permissions it asks for, and what leaves your device."
layout: prose
---
# Privacy

Last changed: 18 November 2026.

**Snake collects no data about you.** It has no accounts, no analytics and no advertising, and the game does not use the network. This page says what that means in practice.

<div class="icl-callout"><p><strong>In one minute</strong>
Snake collects no data about you. It has no account, no analytics and no advertising, and the game does not use the network. Your settings, your scores and the names you typed for the leaderboard are kept in one small file on your device. Nothing leaves your device.</p>
<p><strong>Check it yourself</strong>
The source code is public under the GNU General Public License, version 3 or later.</p></div>

## What the game keeps

The game keeps one small file in its own folder on your device. It holds:

- **Your settings.** How you steer, how sensitive tilt is, whether the axes are inverted, the sound and music switches, the volume and the reduced motion switch.
- **Which modes you have opened.**
- **Your leaderboards.** For each mode, the top 10: the name you typed, the score and the date the game ended. A name is letters A to Z and spaces, up to 12.
- **Your three most recent names**, so you can pick one again.

The file holds no device identifier, location or contacts, and no name other than the ones you type for the leaderboard. On the ESP32-P4 board the same things are kept in the board's own memory.

## What leaves your device

Nothing. The game has no code that connects to the internet. The About screen has a button that opens the game's source page in your browser. Your browser then connects to that page, as it does for any page you open, and the game sends it nothing.

## Permissions

- **iPhone and iPad:** the game asks for no permission. Tilt reads the motion sensor, which needs none. The readings steer the snake while you play. They are not kept.
- **Android:** the game asks for one permission, to vibrate. Android grants it when you install, and it gives the game no access to your data. The game turns off Android's app backup, so the file stays on the device.
- **Computers:** the game asks for no permission.

The game does not use your camera, microphone, location, photos or contacts.

## Deleting what the game keeps

- In Settings, Reset a leaderboard clears the scores of one mode. It asks you to confirm.
- Removing the game from a phone or tablet removes its file. On a computer, delete the game's data folder.

## Children

The game asks for no personal details. The name on the leaderboard is whatever letters the player types, and it stays on the device.

## Changes

If this page changes, the date at the top changes. Questions: see [Support](support.md).
