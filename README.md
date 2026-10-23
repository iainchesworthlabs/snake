# snake

[![CI](https://github.com/iainchesworthlabs/snake/actions/workflows/ci.yml/badge.svg?branch=develop)](https://github.com/iainchesworthlabs/snake/actions/workflows/ci.yml)

A Snake game on Qt 6 and C++23. The game is not implemented yet; the repository holds the structure it will
grow into. It builds with CMake presets and vcpkg, tests with Catch2 and CTest, and uses the shared CI, release
and repository settings in [icl-common-infra](https://github.com/iainchesworthlabs/icl-common-infra).

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

Documentation is in `docs/`. The history of the original 2017 code is in
[iainchesworth/snake.old](https://github.com/iainchesworth/snake.old).

Licence: GPL-3.0-or-later. Contributions need a DCO sign-off (`git commit -s`).
