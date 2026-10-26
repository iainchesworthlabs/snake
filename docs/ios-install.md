# Installing on an iPhone or iPad

Snake is not in the App Store. The way to put a build on a device is **TestFlight**: the release workflow builds the game,
signs it for the App Store, has Apple check it and uploads it, and the people the owner has added as testers install it
from the TestFlight app. No device is registered with Apple and nothing identifies a device in the build, so it suits a
public repository. (An older route, an ad-hoc IPA for registered devices, is at the end.) How the build is made is in
[CI](ci.md#phones-and-tablets), and how the signing pieces come from Apple is in `docs/ios-signing.md` of
`icl-common-infra`.

!!! warning "This has not been tried on a device"
    The signing, the export, Apple's validation and the upload have not run for Snake until the first TestFlight build is
    made, and nobody has run the game on an iPhone or iPad yet. The build's checks (`verify_app.sh --signed --store`) look
    inside the IPA; they do not install it.

## For a tester

1. Install **TestFlight** from the App Store on the iPhone or iPad.
2. Be added as an internal tester by the owner (App Store Connect, the app **ICL Snake**, TestFlight, Internal Testing). The
   Apple ID you sign in to TestFlight with must be the one that was added.
3. Open TestFlight, choose **ICL Snake** and tap Install (or Update). The app is called Snake on the Home Screen.

Needs iOS 18 or later (the build's deployment target).

## For the owner

Set up once (the account, the app record, the App Store profile, the API key and the secrets): `docs/ios-signing.md` in
`icl-common-infra`, "TestFlight". In this repository the pieces are the secrets `IOS_PROVISIONING_PROFILE_BASE64` (an App
Store profile for `com.iainchesworthlabs.snake`) and, in the organisation, `IOS_DIST_CERT_P12_BASE64`,
`IOS_DIST_CERT_PASSWORD`, `APPSTORE_API_KEY_ID`, `APPSTORE_API_ISSUER_ID` and `APPSTORE_API_PRIVATE_KEY`, and the repository
variable `SIGN_IOS_UPLOAD=true`.

```sh
gh workflow run release.yml --ref develop                           # builds, signs, has Apple validate it; uploads nothing
gh workflow run release.yml --ref develop -f upload_testflight=true  # and uploads it to TestFlight
```

A pushed tag uploads too. Each upload gets a build number that is the number of the workflow run, which only grows, as
TestFlight needs. The build appears in TestFlight a few minutes after the upload, once Apple has processed it.

## Ad hoc (private repositories, or a few registered devices)

With `SIGN_IOS_ADHOC=true` and `SIGN_IOS_UPLOAD` unset, the workflow makes an **ad-hoc** IPA instead: it installs only on the
devices that the ad-hoc profile lists, over the air from an `itms-services://` link or from a Mac. The IPA contains its
profile and so the devices' UDIDs, and neither a release asset nor a workflow artifact is private in a public repository, so
this repository uses TestFlight. A new device means a new profile and a new build.
