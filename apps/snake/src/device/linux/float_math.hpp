// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace snake::ui::device
{

// SDL, which is linked into the Linux program as a static library, calls the float functions `atan2f`, `asinf` and
// `sqrtf` of the C library. The Linux package is built on a system with glibc 2.43, which gave those three a new symbol
// version, so the program would then not start on a system with an older glibc (the package is for glibc 2.39 and
// later). The program is linked with `--wrap` for those three names (apps/snake/CMakeLists.txt), so that the calls land
// in the functions below, which work in double and round to float: the double functions have been in glibc for ever.
// The result is the same to the precision of a float, and for the square root it is exactly the same, because the
// double square root rounded to a float is the correctly rounded float square root.

[[nodiscard]] float float_atan2(float y, float x) noexcept;
[[nodiscard]] float float_asin(float value) noexcept;
[[nodiscard]] float float_sqrt(float value) noexcept;

} // namespace snake::ui::device

// The names that the linker puts in place of the C library's: `--wrap=atan2f` sends the references to `atan2f` to
// `__wrap_atan2f`. They are the functions above.
extern "C"
{

    // NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
    [[nodiscard]] float __wrap_atan2f(float y, float x);
    [[nodiscard]] float __wrap_asinf(float value);
    [[nodiscard]] float __wrap_sqrtf(float value);
    // NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)

} // extern "C"
