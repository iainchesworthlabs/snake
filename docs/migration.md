# Migration from the original Snake project

This page records how the original Snake code (the private repository
[iainchesworth/snake.old](https://github.com/iainchesworth/snake.old), written in 2017) became this project. It
is written for developers who want to know where each old file went and what changed.

## What the old project was

A Visual Studio 2017 solution of 15 source files (442 lines) with no tests and no resources. It
set up a logger, a frame timer and a GLFW window description, then ran a 60 frames per second loop that printed
`Frame N`. The code did not compile as it stood, and no window was ever created, because nothing called the
function that creates it. The game was never implemented, and it is not implemented here either.

## What this project is

The same level of function on a current base: a Qt window opens, a 60 Hz game tick runs, and the structure is
ready for game logic. It builds with CMake presets on Linux, macOS and Windows, with C++23, vcpkg for
dependencies and Qt 6 from the shared kit (`ICL_QT_ROOT`) or aqtinstall in CI.

## Layout

| Path | Contents |
|---|---|
| `libs/core` | Qt-free logic: `TimingLoop`, `Logger`, `GameState`, `Game`, window settings. Unit tested with Catch2. |
| `apps/snake` | The Qt Quick application: `main.cpp`, the `GameViewModel` that QML binds to, `Main.qml`, translations. |
| `docs/` | Developer documentation. |

A library never depends on an application, and `libs/core` never includes a Qt header.

## Where the old files went

| Old file | New home | Change |
|---|---|---|
| `EngineCore/TimingLoop` | `libs/core` `TimingLoop` | Same 60 Hz constant and `std::chrono` duration. Members renamed to the project style. |
| `Infrastructure/DebugLogger`, `Interfaces/ILogger` | `libs/core` `Logger` and `StreamLogger` | `OutputDebugString` and the `wchar_t` stream buffer are gone. Messages are UTF-8 and go to a standard stream. Every level now has its own prefix (the old code wrote `[TRACE]` for all six). |
| `Graphics/GraphicsOpenGL`, `Interfaces/IGraphics` | `libs/core` `window_settings` and the Qt Quick window | The 1024 by 768 size, the title and the 4x multisampling are constants in the library. Qt creates the window and chooses the graphics API (OpenGL, Metal or Direct3D) at run time. GLFW is dropped. |
| `WindowManager/WindowManager`, `Interfaces/IWindow` | `apps/snake` `Main.qml` | Empty shells in the old code. The window is a QML `Window`. |
| `main.cpp` | `apps/snake/src/main.cpp`, `libs/core` `Game` | The loop becomes a Qt timer calling `Game::tick`. The frame counter moves into `Game`. |

## What was dropped

- The Visual Studio solution, project and filters files, and the `v141` toolset settings.
- `Dependencies/glfw-3.2.1.bin.*.zip`. The project no longer uses GLFW, and committed binaries do not belong in the history.
- `#pragma comment(lib, ...)`, `Windows.h`, backslash include paths and the empty include-guard macros.
- The reference to `boost_1_64_0.zip` in the project file. No code used Boost.

## Behaviour changes

1. The loop ends when the window closes. The old loop ran until the process was killed.
2. `Frame N` goes to the logger at trace level. The old code wrote it to standard output on every frame.
3. A window opens. In the old code it never did.
4. The tick interval is rounded to whole milliseconds (17 ms) because Qt timers count in milliseconds. The old
   code waited for an exact 1/60 s using `sleep_until`.
5. The graphics API is chosen by Qt. The old code asked for an OpenGL 3.3 core context.

## Decisions

- Qt Quick, not Qt Widgets. The standard asks for QML views over thin view-model classes, and the old window
  was a rendering surface for a game, which fits a scene graph better than a widget.
- Modules: Qt6 Core, Gui, Qml and Quick, plus LinguistTools at build time. Nothing else is linked.
- Languages: English (source) and Simplified Chinese (`zh_CN`). The other languages in the standard are added
  when the window has more text than a title.
- Logging, timing and state have no platform-specific code, so there is no `src/platform/` directory yet. It is
  added with the first function that needs one.

## Provenance

The code was written by Iain Chesworth. History is not copied here; it stays in the original repository.
