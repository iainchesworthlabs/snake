# Building

## Prerequisites

CMake 4.0 or later, Ninja, and a C++23 compiler with `std::expected`: GCC 13 or later, MSVC 19.40
or later, or Apple Clang 16 or later. Clang with libstdc++ older than 14 does not provide
`std::expected`.

## Dependencies

Dependencies come from vcpkg, using the `iainchesworthlabs/vcpkg` fork. The build finds it through
`icl_resolve_vcpkg`, which looks for, in order:

1. `ICL_TEMPLATE_VCPKG_ROOT`, a checkout used as is.
2. `ICL_VCPKG_STORE`, a directory shared by all projects (on this workstation `C:\repos\vcpkg`).
3. `ICL_VCPKG_MIRROR`, a local mirror to clone from when the store is empty.

Qt, when a project uses it, is found through `ICL_TEMPLATE_QT_ROOT` and then `ICL_QT_ROOT`.
Set these once in the shell profile; the presets do not hard-code paths.

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
