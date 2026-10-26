# SPDX-License-Identifier: GPL-3.0-or-later
# What makes the application a program of a phone or a tablet: Android and iOS. Included by apps/snake/CMakeLists.txt when
# ANDROID or IOS is set, by the toolchain files of Qt's kits (the android-* and ios-* presets).
#
# Neither is built the way a desktop program is: there is no `cmake --install`, no CPack and no deploy script.
#
#   Android: qt_add_executable makes a shared library (the activity loads it) and the deployment settings, and
#            androiddeployqt (the `apk` target) turns it, Qt's libraries and plugins, the QML modules and the manifest
#            in packaging/android/package-source into a package, which Gradle signs with Android's debug key.
#   iOS:     Xcode builds a bundle with Qt linked in statically, from packaging/ios/Info.plist.in and the asset
#            catalogue. Unsigned: the build needs no Apple account (a signed build is the owner's, with the owner's
#            accounts and secrets).
#
# Nothing here has run on a device: the CI builds the packages and looks inside them (docs/ci.md, "Phones and tablets").

# Captured here: inside a function CMAKE_CURRENT_LIST_DIR is the caller's.
set(SNAKE_PACKAGING_DIR "${CMAKE_CURRENT_LIST_DIR}")

# The build number (CFBundleVersion) of the iOS application. TestFlight refuses a build whose number is not higher than every
# earlier one for the same version, so the release workflow gives the number of its run. Empty: the project version.
set(SNAKE_IOS_BUILD_NUMBER "" CACHE STRING "Build number of the iOS application (empty: the project version)")
set(SNAKE_IOS_SIGNING_TEAM "" CACHE STRING "Apple team ID the iOS application is signed for (empty: an unsigned build)")
set(SNAKE_IOS_SIGNING_PROFILE "" CACHE STRING "Name of the ad-hoc provisioning profile the iOS application is signed with (empty: an unsigned build)")

# The application id on Android and the bundle identifier on iOS: com.iainchesworthlabs.<app>, as Cubeulator's, which is
# the App ID registered with Apple and what the provisioning profile names. (The macOS bundle has its own identifier,
# apps/snake/CMakeLists.txt.)
set(SNAKE_APPLICATION_ID "com.iainchesworthlabs.snake")
set(SNAKE_DISPLAY_NAME "Snake")

# Android platform levels. 28 is the lowest Qt 6.12 supports. The target and compile levels are the newest Google Play
# takes; a level the SDK in use lacks fails androiddeployqt by name.
set(SNAKE_ANDROID_MIN_SDK 28)
set(SNAKE_ANDROID_TARGET_SDK 36)

# Play wants an integer that only grows, derived from the version so that a tag is enough to make one: 0.2.0 is 20000,
# 0.2.3 is 20300, 1.0.0 is 1000000. A build that must go out under a number of its own passes
# -DSNAKE_ANDROID_VERSION_CODE=...
set(SNAKE_ANDROID_VERSION_CODE "" CACHE STRING "versionCode of the Android package (empty: derived from the project version)")

# The iOS kit's plugin targets that carry no QT_DEFAULT_PLUGIN property: Qt's plugin auto-linking reads it as an operand
# of $<AND:...>, and an empty value is a generate-time error ("Parameters to $<AND> must resolve to either '0' or '1'").
# Whether 6.12's kit has any is unknown, so only a plugin with no value is given one, with what the desktop kits say:
# the ones the game needs are linked, the rest are opt-in. (A function, so that the targets exist: find_package(Qt6)
# made them.)
function(snake_ios_default_plugins)
    foreach(plugin IN ITEMS
            QDebugMessageServiceFactoryPlugin QGifPlugin QICOPlugin QIOSIntegrationPlugin QJpegPlugin
            QLocalClientConnectionFactoryPlugin QQmlDebugServerFactoryPlugin QQmlDebuggerServiceFactoryPlugin
            QQmlInspectorServiceFactoryPlugin QQmlNativeDebugConnectorFactoryPlugin QQmlNativeDebugServiceFactoryPlugin
            QQmlPreviewServiceFactoryPlugin QQmlProfilerServiceFactoryPlugin QQuickProfilerAdapterFactoryPlugin
            QSCNetworkReachabilityNetworkInformationPlugin QSecureTransportBackendPlugin QSvgIconPlugin QSvgPlugin
            QTcpServerConnectionFactoryPlugin)
        if(TARGET Qt6::${plugin})
            get_target_property(default Qt6::${plugin} QT_DEFAULT_PLUGIN)
            if(default STREQUAL "default-NOTFOUND")
                set_target_properties(Qt6::${plugin} PROPERTIES QT_DEFAULT_PLUGIN "1")
            endif()
        endif()
    endforeach()
    # Opt-in only: the other integrations, and the permission plugins of services the game does not use (no usage text
    # exists for them in Info.plist, so linking one would crash the first time its check ran).
    foreach(plugin IN ITEMS
            QDarwinBluetoothPermissionPlugin QDarwinCalendarPermissionPlugin QDarwinCameraPermissionPlugin
            QDarwinContactsPermissionPlugin QDarwinLocationPermissionPlugin QDarwinMicrophonePermissionPlugin
            QIosOptionalPlugin_NSPhotoLibraryPlugin QMinimalIntegrationPlugin QOffscreenIntegrationPlugin
            QTlsBackendCertOnlyPlugin QTuioTouchPlugin)
        if(TARGET Qt6::${plugin})
            get_target_property(default Qt6::${plugin} QT_DEFAULT_PLUGIN)
            if(default STREQUAL "default-NOTFOUND")
                set_target_properties(Qt6::${plugin} PROPERTIES QT_DEFAULT_PLUGIN "0")
            endif()
        endif()
    endforeach()
endfunction()

# Makes the application target a program of this device.
function(snake_configure_mobile_app target)
    if(ANDROID)
        if(SNAKE_ANDROID_VERSION_CODE)
            set(code "${SNAKE_ANDROID_VERSION_CODE}")
        else()
            math(EXPR code
                "${PROJECT_VERSION_MAJOR} * 1000000 + ${PROJECT_VERSION_MINOR} * 10000 + ${PROJECT_VERSION_PATCH} * 100")
        endif()
        # A package source directory is copied over Qt's own template by androiddeployqt: it holds AndroidManifest.xml
        # (what the program asks of the system) and res/ (the launcher icon, the launch colour).
        set_target_properties(${target} PROPERTIES
            QT_ANDROID_PACKAGE_SOURCE_DIR "${SNAKE_PACKAGING_DIR}/android/package-source"
            QT_ANDROID_PACKAGE_NAME "${SNAKE_APPLICATION_ID}"
            QT_ANDROID_APP_NAME "${SNAKE_DISPLAY_NAME}"
            QT_ANDROID_APP_ICON "@mipmap/ic_launcher"
            QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
            QT_ANDROID_VERSION_CODE "${code}"
            QT_ANDROID_MIN_SDK_VERSION "${SNAKE_ANDROID_MIN_SDK}"
            QT_ANDROID_TARGET_SDK_VERSION "${SNAKE_ANDROID_TARGET_SDK}"
            QT_ANDROID_COMPILE_SDK_VERSION "${SNAKE_ANDROID_TARGET_SDK}")
    elseif(IOS)
        if(SNAKE_IOS_BUILD_NUMBER)
            set(SNAKE_IOS_BUILD_NUMBER_OR_VERSION "${SNAKE_IOS_BUILD_NUMBER}")
        else()
            set(SNAKE_IOS_BUILD_NUMBER_OR_VERSION "${PROJECT_VERSION}")
        endif()
        set(assets "${SNAKE_PACKAGING_DIR}/ios/Assets.xcassets")
        set_target_properties(${target} PROPERTIES
            MACOSX_BUNDLE ON
            MACOSX_BUNDLE_INFO_PLIST "${SNAKE_PACKAGING_DIR}/ios/Info.plist.in"
            MACOSX_BUNDLE_BUNDLE_NAME "${SNAKE_DISPLAY_NAME}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "${SNAKE_APPLICATION_ID}"
            MACOSX_BUNDLE_BUNDLE_VERSION "${SNAKE_IOS_BUILD_NUMBER_OR_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_COPYRIGHT "Copyright (C) Iain Chesworth Labs contributors"
            # iPhone and iPad.
            XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
            # The icon set Info.plist.in's CFBundleIconName names; without this Xcode has nothing to compile that slot
            # from and the home screen shows a blank icon.
            XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon")
        # A source path ending in .xcassets is what tells the Xcode generator to compile the catalogue, not copy it.
        target_sources(${target} PRIVATE "${assets}")
        set_source_files_properties("${assets}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
        snake_ios_default_plugins()
        # Signing, for the ad-hoc build only. Both are empty for the unsigned build (the presets turn signing off for every
        # target), and the release workflow gives both from the provisioning profile it checked. They sign the application
        # target alone: signing every Xcode target forces Xcode to sign static libraries and Qt's resource bundles, which
        # have no bundle identifier and were never meant to be signed.
        if(SNAKE_IOS_SIGNING_TEAM AND SNAKE_IOS_SIGNING_PROFILE)
            set_target_properties(${target} PROPERTIES
                XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "YES"
                XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "YES"
                XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual"
                XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Distribution"
                XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${SNAKE_IOS_SIGNING_TEAM}"
                XCODE_ATTRIBUTE_PROVISIONING_PROFILE_SPECIFIER "${SNAKE_IOS_SIGNING_PROFILE}")
        elseif(SNAKE_IOS_SIGNING_TEAM OR SNAKE_IOS_SIGNING_PROFILE)
            message(FATAL_ERROR "SNAKE_IOS_SIGNING_TEAM and SNAKE_IOS_SIGNING_PROFILE go together: one of them is set and the "
                "other is empty. Give both for a signed build, or neither for an unsigned one (docs/building.md, iOS).")
        endif()
    endif()
endfunction()
