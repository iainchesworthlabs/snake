# Snake on the ESP32-P4

The front end for the ESP32-P4 Function EV Board (a 7-inch 1024 by 600 touch panel). It is an ESP-IDF project, built with
`idf.py` and not by the CMake of the rest of the repository. The rules of the game, the input, the sound and the picture of
the board come from the portable libraries in `libs/` (`sim`, `input`, `audio`, `profile` and `raster`), compiled as one
component (`components/snake_portable`); the panel, the touch controller and LVGL come from Espressif's board support package.

## What is here

- A title with the four modes, the game (the board is a canvas that `libs/raster` paints into, with the score on its left,
  the controls on its right, and the Shed button and its fuse in Hot potato) and the end of a game.
- Steering is by thumb, as on a phone: an on-screen joystick (the first choice; it reads eight ways with the phone's own
  rule, `JoystickSteering` in `libs/input`, where a diagonal is a turn across the snake), a d-pad, or a swipe anywhere.
  Chips under the control choose between them. A fourth chip, Tilt, is there and switched off: the board has no motion
  sensor, so tilt waits for the MPU-6050. The choice is kept: the board starts with the one you last chose.
- The profile is saved in the flash's non-volatile storage (`main/save_store.cpp`), in the format that `libs/profile` writes
  for the phones: the way of steering, the unlocks and a leaderboard for each mode. The end of a game shows the best score
  of the mode. There is no keyboard and no clock on the board yet, so every score is saved as `PLAYER` and without a date,
  and the modes are all open: the unlocks are recorded, and nothing is locked behind them.
- No sound, and no tilt: the board has no motion sensor, and the MPU-6050 that will be wired to it is not fitted.
- An Exit button on the title. See "Trying it on a board that is running other firmware" below.

## What has been checked

It **builds**, in CI, with ESP-IDF v6.1 and its GCC 15.2 in Espressif's container (`.github/workflows/esp32-firmware.yml`), and
the board picture is tested on the desktop (`libs/raster`). It has **not run on a board**: nobody has looked at the panel, the
touch response, the speed of the picture or the memory. That is roadmap step 23.

## Building it

```sh
# ESP-IDF v6.1 (https://docs.espressif.com/projects/esp-idf/), with its tools installed and exported
cd apps/snake-esp
idf.py set-target esp32p4
idf.py build
idf.py -p <port> flash monitor
```

For the ESP32-P4X Function EV Board, whose chip is revision v3.x, add the overlay that selects that silicon (an image for a v1.x
chip does not boot on it, and the other way round):

```sh
idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.p4-rev3" set-target esp32p4
idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.p4-rev3" build
```

## Trying it on a board that is running other firmware

Snake has no network of its own, so it cannot update itself and nothing can update it. It also never marks itself as good
(`esp_ota_mark_app_valid_cancel_rollback`). On a board that has two application slots and rolls back (the owner's board, over its
Ethernet), an image of Snake written to the slot that is not running therefore runs until the next restart, and the bootloader
then boots the firmware it came from. The Exit button on the title does that restart at once. A board flashed over USB with
Snake alone has no other slot: Exit does nothing there.

`dependencies.lock` pins the versions of the components that the build fetches (the board support package and LVGL 9.6), so
that a build today is the build of next week.

Snake keeps its profile in the same storage partition as the firmware it was started from, in a namespace of its own (`snake`),
and never erases the partition: when it cannot be opened Snake runs without saving. Nothing of the other firmware's is read
or written.
