# Snake on the ESP32-P4

The front end for the ESP32-P4 Function EV Board (a 7-inch 1024 by 600 touch panel). It is an ESP-IDF project, built with
`idf.py` and not by the CMake of the rest of the repository. The rules of the game, the input, the sound and the picture of
the board come from the portable libraries in `libs/` (`sim`, `input`, `audio`, `profile` and `raster`), compiled as one
component (`components/snake_portable`); the panel, the touch controller and LVGL come from Espressif's board support package.

## What is here

- A title with the four modes, the game (the board is a canvas that `libs/raster` paints into, with the score beside it, and
  the Shed button and its fuse in Hot potato) and the end of a game. A swipe on the touch screen steers.
- Nothing is saved yet: the unlocks and the leaderboards need the flash storage that comes later, so all four modes are open.
- No sound, and no tilt: the board has no motion sensor, and the MPU-6050 that will be wired to it is not fitted.

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

`dependencies.lock` pins the versions of the components that the build fetches (the board support package and LVGL 9.6), so
that a build today is the build of next week.
