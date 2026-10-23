# Building

## Prerequisites

CMake 4.0 or later, Ninja, and a C++23 compiler with `std::expected` and the C++23 library. The project builds
with the floor compilers, which CI installs from the pins in `.github/toolchain-versions.json`: GCC 16, Clang 23
(with GCC 16's libstdc++), MSVC 14.51 (a newer toolset passes) and the newest Apple Clang the macOS runners offer
(Xcode 26.6 on `macos-26`, with Xcode 27 tried in a leg that cannot fail the run). Older compilers may work
but are not tested.

## Dependencies

Dependencies come from vcpkg, using the `iainchesworthlabs/vcpkg` fork, and Qt comes from a prebuilt kit. Both are
found through environment variables with the `ICL_` prefix, which are shared by every project of the organisation;
a variable with the prefix `SNAKE_` overrides it for this project. The presets do not hard-code paths.

| Variable | Meaning |
|---|---|
| `ICL_VCPKG_STORE` | A directory for vcpkg checkouts. The commit named by `builtin-baseline` in `vcpkg.json` gets its own checkout in it (`<store>/<first 12 characters>`), made once and reused by every project that pins the same commit. |
| `ICL_VCPKG_MIRROR` | A clone of the fork to make those checkouts from. Default `<store>/mirror`, cloned on first use. |
| `ICL_QT_ROOT` | The Qt kit, for example `~/Qt/6.12.0/gcc_64`. `QT_ROOT_DIR` and `QTDIR` are the fallbacks. |
| `SNAKE_VCPKG_ROOT`, `SNAKE_QT_ROOT` | A vcpkg checkout or Qt kit used for this project only. |

A configure with none of them set stops and lists the options. Set them once in the shell profile. Qt 6.12 (the
version in `.github/toolchain-versions.json`) is not a vcpkg dependency; install it with aqtinstall.

## Linux

```bash
python3 -m pip install aqtinstall
aqt install-qt linux desktop 6.12.0 linux_gcc_64 -O ~/Qt
export ICL_QT_ROOT=~/Qt/6.12.0/gcc_64 ICL_VCPKG_STORE=~/vcpkg-store
sudo apt install libgl-dev libopengl-dev libegl-dev libxkbcommon-dev libfreetype6 libfontconfig1   # Debian, Ubuntu

cmake --preset linux-release
cmake --build --preset linux-release
ctest --preset linux-release
```

The tests run without a display; `app.smoke` sets `QT_QPA_PLATFORM=offscreen` itself. Use GCC 16 or Clang 23.

## macOS

```bash
python3 -m pip install aqtinstall
aqt install-qt mac desktop 6.12.0 clang_64 -O ~/Qt
export ICL_QT_ROOT=~/Qt/6.12.0/macos ICL_VCPKG_STORE=~/vcpkg-store

cmake --preset macos-release        # Apple Silicon (arm64-osx)
cmake --build --preset macos-release
ctest --preset macos-release
```

The build is a bundle, `snake.app`. Use the Apple Clang of Xcode 26.6 or later.

## Windows

In a PowerShell with the Visual Studio 2026 developer environment (the "Developer PowerShell"), with MSVC toolset
14.51 or newer:

```powershell
python -m pip install "git+https://github.com/miurahr/aqtinstall.git@076e1659807d0b362a3ed684d54c2e9c775eb9c7"
aqt install-qt windows desktop 6.12.0 win64_msvc2022_64 -O C:\repos\common
$env:ICL_QT_ROOT = 'C:\repos\common\6.12.0\msvc2022_64'
$env:ICL_VCPKG_STORE = 'C:\repos\vcpkg-store'

cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release -C Release
```

The PyPI release of aqtinstall cannot read the Windows repository for Qt 6.11 and later; CI installs the commit
named by `aqtsource` in `.github/toolchain-versions.json`. Use `windows-msvc-asan` for AddressSanitizer.

To build without Qt, configure with `-DSNAKE_BUILD_GUI=OFF`; only the library and its tests are built.

## Presets

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

| Preset | Purpose |
|---|---|
| `linux-debug` | Debug with Tracy available (feature `profiling`) |
| `linux-release` | Optimised build |
| `linux-asan-ubsan` | AddressSanitizer and UndefinedBehaviorSanitizer |
| `linux-tsan` | ThreadSanitizer |
| `linux-coverage` | Instrumented build for gcovr |
| `macos-release`, `windows-msvc-release`, `windows-msvc-asan` | Platform builds |

Sanitizers, Tracy, coverage and fuzzing exclude each other. The configure step stops with a
message when two are enabled.

Put personal settings in `CMakeUserPresets.json`, which is ignored by git.
