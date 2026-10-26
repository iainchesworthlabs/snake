# Installing on an iPhone or iPad

Snake is not in the App Store or TestFlight. The release workflow can build an **ad-hoc** IPA: signed for the devices
you have registered with Apple, and installable on those devices only. This page is for the author's own devices. How the
build is made is in [CI](ci.md#phones-and-tablets), and how the signing pieces come from Apple is in `docs/ios-signing.md`
of `icl-common-infra`.

!!! warning "This has not been tried on a device"
    The signing, the export and the install have not run for Snake until the first signed build is made and put on a
    device. The build's checks (`verify_app.sh --signed`) look inside the IPA; they do not install it.

## What you need

- An iPhone or iPad with **iOS 18 or later** (the build's deployment target).
- The device registered in the **ad-hoc provisioning profile** the build was signed with. A device that is not in the
  profile cannot install the app.
- The repository's secrets and the variable `SIGN_IOS_ADHOC` set (see CI).

## Build

```sh
gh workflow run release.yml --ref develop       # a rehearsal: builds and signs, publishes nothing
gh run download <run id> -n ios-adhoc -D ios      # the IPA and manifest.plist
```

A tag builds the same way and, with the repository variable `IOS_IPA_ON_RELEASE` set to `true`, attaches the IPA to the
release.

## Install

- **Over the air.** The IPA and `manifest.plist` must be served over HTTPS: the release's assets are. On the device, in
  Safari, open `itms-services://?action=download-manifest&url=<URL of manifest.plist>`; iOS asks to install and the icon
  fills in on the Home Screen. It has to be Safari. A page on the documentation site with that link is the tidy way, and the
  site is deployed from `main`, which exists after the first final release.
- **From a computer.** With a Mac: Finder (select the device, drag the IPA onto it) or Apple Configurator.

If iOS says the developer is untrusted, go to *Settings, General, VPN & Device Management*, choose the developer and tap
**Trust**. An update is the same again with a newer build: iOS replaces the app and keeps its data.

## Add a device

A device installs only builds signed with a profile that lists it. Register the device in the Apple developer portal, edit
the ad-hoc profile for `io.github.iainchesworthlabs.snake` to include it, download it, set `IOS_PROVISIONING_PROFILE_BASE64`
again and build again. A build made before the device was added never installs on it. A profile lasts a year; when it
expires the build says so before it starts.
