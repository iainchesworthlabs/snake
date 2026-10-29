# ESP32-P4 spike: building with GCC 16

Done 2026-10-28 for roadmap step 20. Nothing here ran on a board: there is no hardware, and Espressif's QEMU has no
ESP32-P4 machine (its RISC-V QEMU lists `esp32c3` only). What was tested is that the code **builds**, and that the
simulation **plays the same games** on RISC-V 32 as on a PC.

## The answer

- **GCC 16 for the ESP32-P4 exists and works.** Espressif's `riscv32-esp-elf` toolchain `esp-16.1.0_20260609`
  (GCC 16.1.0, a release, not a pre-release) is the recommended toolchain of ESP-IDF `master` (v6.2 in development).
  A firmware with the four portable libraries, Espressif's board support package, LVGL 9.6 and `esp_lvgl_port` 2.9.0
  builds with it on `master` with no errors and no warnings of ours.
- **It does not work with the current stable ESP-IDF.** ESP-IDF v6.1 (released 2026-08-27) is built for GCC 15.2.0
  (`esp-15.2.0_20251204`). It refuses another toolchain unless `IDF_MAINTAINER=1` is set, and with GCC 16.1.0 swapped
  in it fails in its own C library header (`esp_libc/platform_include/stdio.h`, a type that the newer C library now
  defines too). So GCC 16 on the chip is tied to ESP-IDF v6.2.
- **The four libraries need no narrowing.** `libs/sim`, `libs/input`, `libs/audio` and `libs/profile` build with
  `-std=c++23` and all the project's warnings as errors on GCC 14.2.0, 15.2.0 and 16.1.0 for `riscv32-esp-elf`, and
  they use nothing from the C++ library that GCC 14 lacks.

## Versions found

| What | Version | Where it is the one |
|---|---|---|
| ESP-IDF stable | v6.1, 2026-08-27 (v6.0.3 and v5.5.6 are maintained) | |
| `riscv32-esp-elf` GCC 15.2.0 | `esp-15.2.0_20251204` | ESP-IDF v6.1 and v6.0 |
| `riscv32-esp-elf` GCC 14.2.0 | `esp-14.2.0_20260121` | ESP-IDF v5.5 |
| `riscv32-esp-elf` GCC 16.1.0 | `esp-16.1.0_20260609` | ESP-IDF `master` (`9c37a0a7`, 2026-10-27) |
| `riscv32-esp-elf` GCC 16.2.0 | `esp-16.2.0_20260914` | pre-release on crosstool-NG, not in any ESP-IDF |
| `esp_lvgl_port` | 2.9.0 (needs ESP-IDF 5.2 or later, LVGL 8 or 9) | |
| LVGL | 9.6.0~1, chosen by the component manager | |
| Board support package `espressif/esp32_p4_function_ev_board` | 5.2.3 ("preview") | display `esp_lcd_ek79007` 2.0.2 or `esp_lcd_ili9881c`, touch `esp_lcd_touch_gt911` 1.2.1, audio `esp_codec_dev` 1.5.11 |
| `espressif/idf` container | `v6.1` (12.9 GB on disk), `latest` is built from `master` | the Dockerfile builds `IDF_CLONE_BRANCH_OR_TAG=master` by default and takes `IDF_INSTALL_TARGETS=esp32p4` to install one chip's tools |

How ESP-IDF builds C++: `-std=gnu++26`, `-fno-exceptions`, RTTI off (both are Kconfig options and off by default), C
as `-std=gnu23`. Since v6.1 the C library is picolibc, and the P4 flags are
`-march=rv32imafc_zicsr_zifencei_zaamo_zalrsc_xesploop_xespv -mabi=ilp32f` (v6.1, GCC 15.2) and
`-march=rv32imafcb_zicsr_zifencei_xesploop_xespv -mabi=ilp32f -mtune=esp32p4` (`master`, GCC 16.1, which also uses the
bit-manipulation extension). The stand-alone GCC 16.1 refuses `-march=...xesppie` (the P4's PIE SIMD extension) that
GCC 14.2 accepts; nothing here needs it.

## What was tried, and the commands

Everything in a scratch directory outside the repository. Toolchains from the tool lists of each ESP-IDF branch
(`tools/tools.json`, which also carries the SHA-256 of each archive):

```sh
# archives come from github.com/espressif/crosstool-NG/releases/download/<name>/riscv32-esp-elf-<ver>-x86_64-linux-gnu.tar.xz
sha256sum -c ...                        # all three matched tools.json
mkdir tc-16.1.0 && tar -xJf riscv32-esp-elf-16.1.0_20260609-x86_64-linux-gnu.tar.xz -C tc-16.1.0
tc-16.1.0/riscv32-esp-elf/bin/riscv32-esp-elf-g++ --version     # (crosstool-NG esp-16.1.0_20260609) 16.1.0
```

### 1. The libraries alone, with the project's flags

Every `.cpp` of the four libraries (21 files), one object each:

```sh
riscv32-esp-elf-g++ -std=c++23 -march=rv32imafc_zicsr_zifencei -mabi=ilp32f -Os \
  -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections \
  -Wall -Wextra -Wpedantic -Werror -Wshadow -Wcast-align -Wunused -Wconversion -Wsign-conversion \
  -Wnull-dereference -Wdouble-promotion -Wimplicit-fallthrough -Wformat=2 \
  -Wnon-virtual-dtor -Woverloaded-virtual -Wold-style-cast \
  -Ilibs/sim/include -Ilibs/input/include -Ilibs/audio/include -Ilibs/profile/include -Ilibs/input/src \
  -c libs/<lib>/src/<file>.cpp -o <lib>-<file>.o
```

| Toolchain | Files built | Failed | Code size (`text`, all 21 objects) |
|---|---|---|---|
| GCC 14.2.0 | 21 | 0 | 17 002 bytes |
| GCC 15.2.0 | 21 | 0 | 17 018 bytes |
| GCC 16.1.0 | 21 | 0 | 17 094 bytes |

By library at GCC 16.1.0: sim 9 482, input 1 510, audio 2 646, profile 3 456 bytes. The three compilers are within
half a percent of each other, so the newer compiler costs nothing in size. Speed was not measured: it needs the chip.
The hardening flag `-fstack-protector-strong` is left out: ESP-IDF has its own option for it.

C++ library features, one small file each, built with `-std=c++23` and `-std=c++26` (`-fno-exceptions -fno-rtti`):

| Feature | GCC 14.2 | GCC 15.2 | GCC 16.1 |
|---|---|---|---|
| `<expected>`, `<span>`, `<ranges>`, `<bit>`, `<format>`, `<print>`, `<generator>`, `<stacktrace>` | yes | yes | yes |
| deducing `this`, `static operator()` lambdas | yes | yes | yes |
| `<flat_map>` | no header | yes | yes |
| `<mdspan>` | no header | no header | yes |
| `<inplace_vector>` | no | no | with `-std=c++26` only |

The libraries use none of the last three. That a header compiles is not that it is a good idea on the chip:
`<print>`, `<format>` and `<stacktrace>` pull in code the firmware would not want, and the portable libraries'
include allowlist (standard 3.1, rule 7) already keeps them out.

### 2. Do they play the same games? (no board: user-mode QEMU)

`qemu-riscv32` runs a RISC-V 32 Linux binary on a PC. The probe plays the autoplayer for 8 seeds, both edge rules,
and prints a hash of each end state (snake cells, score, ticks, won). It was built with `libs/sim` only, with no C
library (own `_start`, `write` and `exit` as Linux system calls), and run on the PC and under QEMU:

```sh
g++ -std=c++23 -O2 -fno-exceptions -fno-rtti -Ilibs/sim/include golden.cpp libs/sim/src/*.cpp -o golden-host
riscv32-esp-elf-g++ -std=c++23 -O2 -mno-relax -msmall-data-limit=0 -fno-tree-loop-distribute-patterns \
  -march=rv32imac_zicsr_zifencei -mabi=ilp32 -DRISCV_USER -fno-exceptions -fno-rtti \
  -nostartfiles -nodefaultlibs -static -Ilibs/sim/include golden.cpp libs/sim/src/*.cpp -lgcc -o golden-rv32
qemu-riscv32 ./golden-rv32 | cmp - <(./golden-host)
```

Result: **identical output for GCC 14.2.0, 15.2.0 and 16.1.0 at `-Os`, `-O1`, `-O2` and `-O3`** (12 builds). This is
the evidence for the portable libraries' "same result on every platform" rule on a 32-bit target. The three flags
after `-O2` only keep the probe's own start-up code honest (no global pointer is set up without the C library's
start file, and a hand-written `memcpy` must not be turned into a call to itself); they are not needed in a firmware.
It does not replace running the suites on the chip: it is one game per seed and covers `libs/sim` only.

### 3. A firmware with the libraries, the board support package and LVGL

A throwaway ESP-IDF project: the four libraries as one component, and a `main.cpp` that plays an autoplayer game,
then shows the score in an LVGL label through the board support package. The component:

```cmake
# components/snake_portable/CMakeLists.txt     (SNAKE_ROOT is a checkout of snake)
foreach(lib sim input audio profile)
    file(GLOB srcs "${SNAKE_ROOT}/libs/${lib}/src/*.cpp")
    list(APPEND all_srcs ${srcs})
endforeach()
idf_component_register(SRCS ${all_srcs}
    INCLUDE_DIRS ${SNAKE_ROOT}/libs/sim/include ${SNAKE_ROOT}/libs/input/include
                 ${SNAKE_ROOT}/libs/audio/include ${SNAKE_ROOT}/libs/profile/include)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Wpedantic -Werror -Wshadow ... )   # the list above
```

`main/idf_component.yml` is `dependencies: { espressif/esp32_p4_function_ev_board: "*" }`, which brings in
`esp_lvgl_port`, LVGL, the panel and touch drivers and the audio codec driver.

```sh
# 3a. ESP-IDF v6.1 and its GCC 15.2.0, in Espressif's container
docker pull espressif/idf:v6.1
docker run --rm -v $PWD/proj:/project -v $PWD/snake:/snake:ro -w /project espressif/idf:v6.1 \
  bash -lc 'idf.py set-target esp32p4 && idf.py build'

# 3b. the same container with GCC 16.1.0 mounted over it
docker run --rm -e IDF_MAINTAINER=1 ... -v $PWD/tc-16.1.0/riscv32-esp-elf:/tc16:ro espressif/idf:v6.1 \
  bash -lc 'export PATH=/tc16/bin:$PATH; idf.py set-target esp32p4 && idf.py build'

# 3c. ESP-IDF master: what the `latest` container is built from. Inside the v6.1 container, in a work directory:
git clone --depth 1 --recursive --shallow-submodules https://github.com/espressif/esp-idf.git idf
IDF_TOOLS_PATH=$PWD/tools IDF_PYTHON_ENV_PATH=$PWD/pyenv ./idf/install.sh esp32p4   # fetches GCC 16.1.0
. ./idf/export.sh                                                                  # cmake: put it on PATH by hand
idf.py set-target esp32p4 && idf.py build
```

| Build | Result |
|---|---|
| 3a, ESP-IDF v6.1, GCC 15.2.0, libraries only | builds; 197 KB image at the default `-Og`; no warnings from our sources |
| 3a, same, with the board support package, LVGL 9.6 and `esp_lvgl_port` | builds, no warnings from our sources |
| 3b, v6.1 with GCC 16.1.0 | **fails**: `IDF_MAINTAINER=1` is needed to get past the toolchain check, then `stdio.h` of ESP-IDF's own `esp_libc` conflicts with the newer C library (`cookie_io_functions_t`, `fopencookie`) in every C file |
| 3c, ESP-IDF master, GCC 16.1.0, libraries only | builds, no warnings |
| 3c, same, with the board support package, LVGL 9.6 and `esp_lvgl_port` | builds; the only warnings are two deprecation notes in Espressif's `esp_video` component |

Not tried: running anything on the chip; the rendering speed of LVGL; the RAM and flash a full game needs; the MPU-6050
driver (not fitted); the licences of the Espressif components (looked up in step 21).

## Options for the toolchain floor on the ESP

The standard's D7 floor is GCC 16, and it lets an embedded toolchain be older. The spike says:

| Option | For | Against |
|---|---|---|
| A. ESP-IDF v6.1 with its GCC 15.2.0 (stable and supported) | Works today, in Espressif's container. One compiler version behind the floor. | The firmware is built with a compiler below D7 until v6.2. |
| B. ESP-IDF master with GCC 16.1.0 | Meets D7 and the project's plan. | Master moves daily; a firmware that builds today may not next week. No release to pin: a commit would be pinned. |
| C. A on the pull request gate, B in the nightly leg | Stable gate, early warning of what v6.2 will break, and the day v6.2 is released the pin moves up. | Two legs to keep. |

## Recommended next steps

1. **Take option C for step 21** and pin `esp32p4` toolchain keys in `.github/toolchain-versions.json` (the version of
   ESP-IDF, the toolchain name, the container digest) as the standard's rule 5 asks.
2. **Amend the standard (step 21).** Rule 5 of section 3.1 can record the result: the narrowing to a GCC 14 subset is
   not needed for the four libraries (built and checked on 14.2, 15.2 and 16.1). D7 stays as it is, with the ESP row
   saying "GCC 15.2 on ESP-IDF v6.1 until v6.2, then the floor".
3. **CI for the libraries without a container.** The toolchain is a 150 MB archive with a published SHA-256, so a
   job can download it, check the hash and run the loop in section 1 in seconds. This is the leg for the pull request
   gate when a portable library changes (rule 7c). Add `qemu-user` and the probe of section 2 to it (with the
   golden vectors of the existing tests rather than my autoplayer hashes).
4. **CI for the firmware in Espressif's container.** Pin the image by digest (`espressif/idf:v6.1@sha256:81893c71...`,
   12.9 GB on disk, so it is slow to pull: cache it, or build a smaller image from Espressif's Dockerfile with
   `IDF_INSTALL_TARGETS=esp32p4`). Run it as the runner's user with `HOME` set: as root it leaves root-owned build
   files that the runner cannot clean. A job runs `idf.py set-target esp32p4 && idf.py build` and checks the sizes;
   no hardware is needed.
5. **Step 22.** Take the board support package, `esp_lvgl_port` and LVGL 9 as found; the panel (EK79007 or ILI9881C,
   1024 by 600) and the GT911 touch controller are named in the package, which settles the questions the roadmap left
   open. The tilt source stays the fake replay until the MPU-6050 is fitted. Audio goes through `esp_codec_dev` (the
   ES8311) from the `libs/audio` synthesiser.
6. **Step 23** is on hardware only. Record frame rate, the time of a tick, RAM and flash, and run the library suites
   on the chip (Unity or Catch2 on target) as well as the host.
