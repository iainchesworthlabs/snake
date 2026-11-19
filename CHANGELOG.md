# Changelog

All notable changes are listed here. Each release section is assembled from the
fragments in `changelog.d/`.

## [1.0.0] - 2026-11-19

### Added

- **Snake, with four modes.** Easy is slower and its walls wrap around; Classic is faster and a wall ends the game. Eat the food to grow and to score, and take the bonus item when it appears. Portals and Hot potato open at 2,000 and 3,000 points in Classic.

- **A game you can play from the title to the leaderboard.** A title screen where you choose a mode, a pause dialog, and a game over screen that tells you how close you were to your best. A score that makes the top 10 asks for a name, and each mode has its own leaderboard. Scores, names and settings are kept between runs.

- **The Toybox Pixel look, with sound.** A striped snake, a blueberry that bobs, bursts of specks and rising points, a shake when you crash and a short death scene. The sound effects and the music are made by the game itself. Settings has sound and music switches and a reduced motion option, and About tells you the version, the licence and where the source is.

- **Eight languages.** English, French, Spanish, German, Japanese, Simplified Chinese, Hebrew and Yiddish, chosen from the languages your device prefers, with the right-to-left languages mirrored. The translations are made by machine and have not been read by someone who speaks the language.

- **On a phone or a tablet.** Hold it upright or on its side: every screen has a layout for each, and the screen is held still during a game so that tilting never turns it. (Landscape has been checked in screenshots, not yet on a phone.) Steer by tilting it (with a short calibration, and a choice of how sensitive), by swiping, with an on-screen d-pad or with an on-screen joystick that reads eight ways. The phone taps when the snake eats and knocks when it crashes, and the game keeps inside the camera cut-out and the home bar. An iPhone and iPad version goes out through TestFlight; the Android package builds but has not been tried on a device.

- **Downloads for Linux, macOS and Windows.** Each release has a package for each, with checksums, signatures, a list of what is in it and a proof of where it was built. The Windows files are test-signed and the macOS app is not signed or notarised yet.

- **A front end for the ESP32-P4 board.** The board painter that the chip's screen uses is now a tested library, and an ESP-IDF project with a title, the game, the Shed button and a game-over screen builds from it. It has not run on a board yet, so there is nothing to install.

- **The ESP32-P4 board remembers.** The way you chose to steer is kept for the next start, and the end of a game shows the best score of the mode, which is saved with the unlocks in the board's flash. Every score is saved under one name for now, because the board has no keyboard yet. It has not run on a board yet. (The words are English only for now.)

- **Touch controls on the ESP32-P4 board.** The board's game has the phone's on-screen joystick, a d-pad and swiping, with the joystick first, and the score on the left, the board in the middle and the controls on the right. Tilt is on the list and switched off, because the board has no motion sensor. It has not run on a board yet. (The words are English only for now.)

- **Gamepads work on a computer.** Plug in or pair a gamepad and it plays the game and moves through the menus: the d-pad or the left stick steers (and moves between buttons), Start pauses and resumes, Select or B opens the menu, and A presses the button that has the focus. (Not tried on a real pad yet; phones and tablets do not read pads yet.)

- **Key hints on a computer.** Lying down, the game now shows what the keys do beside the board: W A S D or the arrow keys to steer, Space to pause, Esc for the menu and, in Hot potato, Enter to shed the tail. (The new words are machine translations.)

- **Portals and Hot potato.** In Portals, two pairs of portals sit on the board: step into one and you come out of its twin, going the same way. In Hot potato, a bomb food now and then appears: eat it for 100 points and the bomb rolls down your body towards your tail, and you must press Shed (the button, or Enter, or A on a gamepad) to drop it, and the part of your tail behind it, before it goes off. Each has its own leaderboard, and a new sound for the portal, the bomb, the shed and the explosion. (The new words are machine translations, not yet read by a speaker of each language.)

- **A volume setting.** Settings has a volume from silent to full, turned down and up with the minus and plus buttons or the left and right arrow keys, and kept with the other settings. Sound and Music are still there on top of it. (Saves from the earlier builds still open, at full volume.)

### Fixed

- **The Linux package starts on Ubuntu 24.04 and Debian 13.** The first release candidate was built so that it needed a newer system than those; the release now checks that nothing in the package does.

