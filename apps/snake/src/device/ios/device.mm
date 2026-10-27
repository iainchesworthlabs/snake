// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/device.hpp"

#include "device/haptics.hpp"
#include "device/qt_accelerometer.hpp"
#include "snake/input/sample_source.hpp"

#import <UIKit/UIKit.h>
#import <objc/runtime.h>

#include <memory>

namespace snake::ui::device
{

namespace
{

/// The taptic engine, through UIImpactFeedbackGenerator: a light impact for eating and a heavy one for a crash. A phone
/// without the engine (an iPad) does nothing.
class IosHaptics final : public Haptics
{
public:
    void tap() override
    {
        impact(UIImpactFeedbackStyleLight);
    }

    void thud() override
    {
        impact(UIImpactFeedbackStyleHeavy);
    }

private:
    static void impact(UIImpactFeedbackStyle style)
    {
        @autoreleasepool
        {
            UIImpactFeedbackGenerator* generator = [[UIImpactFeedbackGenerator alloc] initWithStyle:style];
            [generator impactOccurred];
        }
    }
};

/// What the application's delegate answers when UIKit asks which orientations a window may be in. It is the whole
/// screen's say: while a game is on it is the orientation the screen was in when the game began, and otherwise what the
/// package's property list allows (an iPad every one, an iPhone all but upside down).
UIInterfaceOrientationMask g_LockedMask = UIInterfaceOrientationMaskAllButUpsideDown;
bool g_Locked = false;

UIInterfaceOrientationMask unlocked_mask()
{
    return UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad
               ? UIInterfaceOrientationMaskAll
               : UIInterfaceOrientationMaskAllButUpsideDown;
}

UIInterfaceOrientationMask supported_orientations(id /*self*/, SEL /*cmd*/, UIApplication* /*application*/,
                                                  UIWindow* /*window*/)
{
    return g_Locked ? g_LockedMask : unlocked_mask();
}

/// The orientation of the interface now, as a mask of that one orientation, or all that are allowed when there is no
/// window scene to ask.
UIInterfaceOrientationMask current_mask()
{
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes)
    {
        if ([scene isKindOfClass:UIWindowScene.class])
        {
            const UIInterfaceOrientation orientation = static_cast<UIWindowScene*>(scene).interfaceOrientation;
            if (orientation != UIInterfaceOrientationUnknown)
            {
                return static_cast<UIInterfaceOrientationMask>(1U << static_cast<NSUInteger>(orientation));
            }
        }
    }
    return unlocked_mask();
}

} // namespace

std::unique_ptr<input::SampleSource> make_accelerometer()
{
    return make_qt_accelerometer();
}

std::unique_ptr<Haptics> make_haptics()
{
    return std::make_unique<IosHaptics>();
}

void lock_orientation(bool locked)
{
    // UIKit asks the application delegate (and the property list) which orientations are allowed, and Qt's delegate
    // does not take a say, so the answer is given here: the method is put on the delegate's class the first time, and
    // after that it only reads the mask above. The system never rotates a screen to an orientation that the delegate
    // refuses.
    static bool installed = false;
    if (!installed)
    {
        const Class delegate = [UIApplication.sharedApplication.delegate class];
        if (delegate != Nil)
        {
            class_replaceMethod(delegate, @selector(application:supportedInterfaceOrientationsForWindow:),
                                reinterpret_cast<IMP>(&supported_orientations), "Q@:@@");
            installed = true;
        }
    }
    g_LockedMask = current_mask();
    g_Locked = locked;
    if (!locked)
    {
        // The phone may have been turned while the screen was held: let it catch up now.
        [UIViewController attemptRotationToDeviceOrientation];
    }
}

} // namespace snake::ui::device
