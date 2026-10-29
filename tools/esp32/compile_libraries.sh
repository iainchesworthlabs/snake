#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds the portable libraries (libs/sim, libs/input, libs/audio, libs/profile) with Espressif's RISC-V toolchain for the
# ESP32-P4, to show that they compile with it under the project's warnings as errors, with exceptions and RTTI off.
#
#     tools/esp32/compile_libraries.sh <toolchain> <sha256> [work-directory]
#
# <toolchain> is the name of the release of Espressif's crosstool-NG (esp-15.2.0_20251204) and <sha256> the hash of its
# archive for x86-64 Linux; both come from the "esp32p4" entry of .github/toolchain-versions.json, which gets them from the
# tool list of ESP-IDF (tools/tools.json). The archive is downloaded into the work directory (a temporary one by default),
# checked against the hash before it is unpacked, and the sources are compiled, one object each. Nothing is linked and
# nothing runs: there is no board.
set -euo pipefail

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
    echo "usage: $0 <toolchain> <sha256> [work-directory]" >&2
    exit 2
fi
toolchain="$1"
sha256="$2"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
work="${3:-$(mktemp -d)}"
mkdir -p "$work"

version="${toolchain#esp-}"
archive="riscv32-esp-elf-${version}-x86_64-linux-gnu.tar.xz"
url="https://github.com/espressif/crosstool-NG/releases/download/${toolchain}/${archive}"

if [ ! -f "$work/$archive" ]; then
    echo "Downloading $url"
    curl --fail --location --silent --show-error --retry 3 --output "$work/$archive" "$url"
fi
echo "$sha256  $work/$archive" | sha256sum --check --status || {
    echo "::error::$archive does not have the hash that .github/toolchain-versions.json pins" >&2
    exit 1
}
if [ ! -d "$work/${toolchain}" ]; then
    mkdir "$work/${toolchain}"
    tar -xJf "$work/$archive" -C "$work/${toolchain}"
fi

bin="$work/${toolchain}/riscv32-esp-elf/bin"
cxx="$bin/riscv32-esp-elf-g++"
"$cxx" --version | head -n 1

# The project's warnings (cmake/icl), as errors. ESP-IDF sets its own stack protection and optimisation, so those are left out.
warnings=(-Wall -Wextra -Wpedantic -Werror -Wshadow -Wcast-align -Wunused -Wconversion -Wsign-conversion
          -Wnull-dereference -Wdouble-promotion -Wimplicit-fallthrough -Wformat=2 -Wnon-virtual-dtor
          -Woverloaded-virtual -Wold-style-cast)
# The ESP32-P4's core with the single-precision FPU, which every ESP-IDF toolchain accepts.
target=(-march=rv32imafc_zicsr_zifencei -mabi=ilp32f)
includes=()
for lib in sim input audio profile; do
    includes+=("-I$root/libs/$lib/include")
done
includes+=("-I$root/libs/input/src")

objects="$work/objects-${toolchain}"
rm -rf "$objects"
mkdir -p "$objects"
count=0
for lib in sim input audio profile; do
    for source in "$root"/libs/"$lib"/src/*.cpp; do
        object="$objects/$lib-$(basename "$source" .cpp).o"
        "$cxx" -std=c++23 "${target[@]}" -Os -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections \
            "${warnings[@]}" "${includes[@]}" -c "$source" -o "$object"
        count=$((count + 1))
    done
done
echo "$count files compiled with $toolchain"
"$bin/riscv32-esp-elf-size" "$objects"/*.o | awk 'NR > 1 { text += $1; data += $2; bss += $3 } END { print "text " text ", data " data ", bss " bss " bytes in all" }'
