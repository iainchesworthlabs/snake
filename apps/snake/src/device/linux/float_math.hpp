// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace snake::ui::device
{

// SDL, which is linked into the Linux program as a static library, calls the float functions of the C library (its file
// SDL_stdlib.c holds the calls: `atan2f`, `asinf`, `sqrtf` and the rest below). The Linux package is built on a system
// with glibc 2.43, which gave some of those a new symbol version, so the program would then not start on a system with
// an older glibc (the package is for glibc 2.39 and later). The program is linked with `--wrap` for these names
// (apps/snake/CMakeLists.txt, where the list is), so that the calls land in the functions below, which work in double
// and round to float: the double functions have been in glibc for ever. The result is the same to the precision of a
// float, and for the square root it is exactly the same, because the double square root rounded to a float is the
// correctly rounded float square root.

[[nodiscard]] float float_acos(float value) noexcept;
[[nodiscard]] float float_asin(float value) noexcept;
[[nodiscard]] float float_atan(float value) noexcept;
[[nodiscard]] float float_atan2(float y, float x) noexcept;
[[nodiscard]] float float_cos(float value) noexcept;
[[nodiscard]] float float_exp(float value) noexcept;
[[nodiscard]] float float_log(float value) noexcept;
[[nodiscard]] float float_log10(float value) noexcept;
[[nodiscard]] float float_pow(float base, float exponent) noexcept;
[[nodiscard]] float float_sin(float value) noexcept;
[[nodiscard]] float float_sqrt(float value) noexcept;
[[nodiscard]] float float_tan(float value) noexcept;

} // namespace snake::ui::device

// The names that the linker puts in place of the C library's: `--wrap=atan2f` sends the references to `atan2f` to
// `__wrap_atan2f`. Each is the function above of the same name.
extern "C"
{

    // NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
    [[nodiscard]] float __wrap_acosf(float value);
    [[nodiscard]] float __wrap_asinf(float value);
    [[nodiscard]] float __wrap_atanf(float value);
    [[nodiscard]] float __wrap_atan2f(float y, float x);
    [[nodiscard]] float __wrap_cosf(float value);
    [[nodiscard]] float __wrap_expf(float value);
    [[nodiscard]] float __wrap_logf(float value);
    [[nodiscard]] float __wrap_log10f(float value);
    [[nodiscard]] float __wrap_powf(float base, float exponent);
    [[nodiscard]] float __wrap_sinf(float value);
    [[nodiscard]] float __wrap_sqrtf(float value);
    [[nodiscard]] float __wrap_tanf(float value);
    // NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)

} // extern "C"
