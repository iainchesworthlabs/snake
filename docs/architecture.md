# Architecture

```text
libs/core/            Qt-free library: timing, logging, game state; public headers, sources, tests
  include/snake/
  src/
  tests/
apps/snake/           Qt Quick application
  src/                main.cpp
  src/viewmodel/      thin QObject classes that QML binds to (camelCase names, see below)
  qml/                Main.qml
  translations/       snake_<language>.ts
  tests/              view-model and translation tests
cmake/icl/            vendored CMake modules from icl-common-infra (do not edit)
tools/                project scripts, baselines and coverage floors
```

## Platform code

There is no platform-specific code yet. When there is, the differences live in `src/platform/<os>/` with
the same file names in each directory.
`icl_platform_sources(target)` adds the directory that matches the target system. Source files
contain no `#if` on the platform or compiler; the preprocessor check fails the pull request if
one appears. Other directives are counted per file against `tools/preprocessor-baseline.json`,
which can go down but not up.

## Naming

Namespaces, functions and variables use `snake_case`. Types use `PascalCase`. Members use
`m_PascalCase`. Constants and enumerators use `UPPER_SNAKE_CASE`. Braces follow Allman style.
`.clang-format` and `.clang-tidy` enforce this.

## View-models

QML sees the game through `snake::ui::GameViewModel`, which holds a `snake::Game` and a timer and has no
rules of its own. Names that QML sees (properties, signals, invokable functions) are camelCase; the
override is in `apps/snake/src/viewmodel/.clang-tidy`. A bulk change emits one signal, not one per item.

## Vendored modules

`cmake/icl/` is a copy of `icl-common-infra/cmake/`. `cmake/icl/SOURCE` records the commit.
CI compares the copy with the commit its workflows pin and fails on a difference. Update with
`python3 tools/ci/sync_cmake.py --infra <checkout of icl-common-infra>`.
