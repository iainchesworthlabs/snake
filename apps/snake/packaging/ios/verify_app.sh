#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Check that an iOS application (a built .app, or an exported .ipa) is complete.
#
#   apps/snake/packaging/ios/verify_app.sh <snake.app | package.ipa>
#       [--version <x.y.z>] [--signed]
#
# What it looks for:
#
#   * Info.plist is valid, names the identifier, the executable, the version, the
#     icon set and a launch screen, says it is an iPhone/iPad program, and holds
#     no usage text for a permission (the game asks for none);
#   * the compiled asset catalogue (the icon) is in the bundle;
#   * the executable is for arm64 and links nothing outside the system: Qt is
#     static on iOS, so a link to a Qt framework or to a path on the build
#     machine means the wrong kit was used;
#   * with --signed: the code signature verifies, the profile is embedded, it is an
#     ad-hoc one (it names devices and does not allow a debugger to attach) and it
#     belongs to this bundle identifier.
#
# macOS only (plutil, otool, codesign, security).
set -euo pipefail

target=""
version=""
signed=false
while [ $# -gt 0 ]; do
  case "$1" in
    --version) version="${2:?}"; shift 2 ;;
    --signed) signed=true; shift ;;
    -h|--help) sed -n '2,22p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*) echo "verify_app.sh: unknown argument '$1'" >&2; exit 2 ;;
    *) target="$1"; shift ;;
  esac
done
[ -e "$target" ] || { echo "usage: verify_app.sh <app|ipa> [--version v] [--signed]" >&2; exit 2; }
fail() { echo "FAILED: $*" >&2; exit 1; }

bundle_id="io.github.iainchesworthlabs.snake"

work=""
cleanup() { if [ -n "$work" ]; then rm -rf "$work"; fi; }
trap cleanup EXIT

app="$target"
case "$target" in
  *.ipa)
    work="$(mktemp -d)"
    unzip -q "$target" -d "$work"
    app="$(find "$work/Payload" -maxdepth 1 -name '*.app' -type d | head -n 1)"
    [ -n "$app" ] || fail "no Payload/*.app in $target"
    ;;
esac
echo "==> $app"

plist="$app/Info.plist"
[ -f "$plist" ] || fail "no Info.plist (an iOS bundle is flat: it is beside the executable)"
plutil -lint "$plist" >/dev/null || fail "Info.plist is not a valid property list"
key() { /usr/libexec/PlistBuddy -c "Print :$1" "$plist" 2>/dev/null || true; }

[ "$(key CFBundleIdentifier)" = "$bundle_id" ] || fail "CFBundleIdentifier is '$(key CFBundleIdentifier)'"
[ "$(key CFBundleExecutable)" = "snake" ] || fail "CFBundleExecutable is '$(key CFBundleExecutable)'"
for usage in NSCameraUsageDescription NSMicrophoneUsageDescription NSLocationWhenInUseUsageDescription NSPhotoLibraryUsageDescription; do
  [ -z "$(key $usage)" ] || fail "Info.plist has $usage: the game asks for no permission"
done
[ "$(key CFBundleIconName)" = "AppIcon" ] || fail "CFBundleIconName is '$(key CFBundleIconName)'"
[ -n "$(key UILaunchScreen)" ] || fail "Info.plist has no UILaunchScreen (iOS would run the program in a legacy window)"
[ "$(key DTPlatformName)" = "iphoneos" ] || fail "DTPlatformName is '$(key DTPlatformName)'; this is not a device build"
[ -n "$(key MinimumOSVersion)" ] || fail "Info.plist has no MinimumOSVersion"
if [ -n "$version" ]; then
  [ "$(key CFBundleShortVersionString)" = "$version" ] || fail "CFBundleShortVersionString is '$(key CFBundleShortVersionString)', expected $version"
fi
echo "    Info.plist: identifier, executable, version, icon, launch screen, no permission text, iOS $(key MinimumOSVersion) or later"

for required in \
    "$app/snake" \
    "$app/Assets.car"; do
  [ -e "$required" ] || fail "missing ${required#"$app"/}"
done
echo "    executable and asset catalogue"

archs="$(lipo -archs "$app/snake" 2>/dev/null || true)"
[ "$archs" = "arm64" ] || fail "the executable is for '$archs', expected arm64"
bad="$(otool -L "$app/snake" | tail -n +2 | awk '{print $1}' | grep -Ev '^(/usr/lib/|/System/Library/)' || true)"
[ -z "$bad" ] || fail "the executable links outside the system: $bad"
echo "    executable: arm64, links only the system"

if $signed; then
  codesign --verify --strict --verbose=2 "$app" || fail "the code signature does not verify"
  profile="$app/embedded.mobileprovision"
  [ -f "$profile" ] || fail "no embedded.mobileprovision: the build was not signed with a profile"
  decoded="$(mktemp)"
  security cms -D -i "$profile" > "$decoded" 2>/dev/null || fail "cannot decode embedded.mobileprovision"
  pkey() { /usr/libexec/PlistBuddy -c "Print :$1" "$decoded" 2>/dev/null || true; }
  case "$(pkey Entitlements:application-identifier)" in
    *."$bundle_id") ;;
    *) fail "the profile is for '$(pkey Entitlements:application-identifier)', not $bundle_id" ;;
  esac
  devices="$(/usr/libexec/PlistBuddy -c "Print :ProvisionedDevices" "$decoded" 2>/dev/null | grep -c '^    ' || true)"
  [ "$devices" -gt 0 ] || fail "the profile names no devices: it is not an ad-hoc profile"
  [ "$(pkey Entitlements:get-task-allow)" = "false" ] || fail "the profile allows a debugger to attach (get-task-allow): it is a development profile"
  echo "    signed: the signature verifies; ad-hoc profile '$(pkey Name)' for $devices device(s), expires $(pkey ExpirationDate)"
  rm -f "$decoded"
fi
echo "OK: $target"
