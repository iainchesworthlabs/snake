// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/device.hpp"

#include "device/game_center.hpp"
#include "device/gamepad.hpp"
#include "device/haptics.hpp"
#include "device/qt_accelerometer.hpp"
#include "snake/input/sample_source.hpp"

#import <GameKit/GameKit.h>
#import <UIKit/UIKit.h>
#import <dispatch/dispatch.h>
#import <objc/runtime.h>

#include <cstdint>
#include <memory>
#include <string_view>

/// Closes Game Center's own screen when the player is done with it.
@interface SnakeGameCenterDelegate : NSObject <GKGameCenterControllerDelegate>
@end

@implementation SnakeGameCenterDelegate

- (void)gameCenterViewControllerDidFinish:(GKGameCenterViewController*)gameCenterViewController
{
    [gameCenterViewController dismissViewControllerAnimated:YES completion:nil];
}

@end

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

/// The view controller in front, to put Game Center's screens on, or nil when there is no window yet.
UIViewController* front_view_controller()
{
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes)
    {
        if ([scene isKindOfClass:UIWindowScene.class])
        {
            for (UIWindow* window in static_cast<UIWindowScene*>(scene).windows)
            {
                if (window.isKeyWindow)
                {
                    UIViewController* controller = window.rootViewController;
                    while (controller.presentedViewController != nil)
                    {
                        controller = controller.presentedViewController;
                    }
                    return controller;
                }
            }
        }
    }
    return nil;
}

/// Puts `controller` in front of the game. At start-up the window may not be there yet, so it tries again a few times,
/// a second apart, and gives up after that (the player is asked again the next time the game starts).
void present_when_possible(UIViewController* controller, int tries_left)
{
    UIViewController* front = front_view_controller();
    if (front != nil)
    {
        [front presentViewController:controller animated:YES completion:nil];
        return;
    }
    if (tries_left > 0)
    {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(NSEC_PER_SEC)), dispatch_get_main_queue(),
                       ^{
                         present_when_possible(controller, tries_left - 1);
                       });
    }
}

NSString* to_string(std::string_view text)
{
    return [[NSString alloc] initWithBytes:text.data() length:text.size() encoding:NSUTF8StringEncoding];
}

/// Game Center through GameKit: the player signs in when the game starts (Game Center asks, once, if nobody is signed
/// in), scores go to the leaderboards that App Store Connect holds, and its own screen shows them.
class IosGameCenter final : public GameCenter
{
public:
    IosGameCenter()
    {
        // GameKit calls this on the main thread, at start-up and whenever the player signs in or out. When someone has
        // to sign in it gives a screen for it, which is put in front of the game.
        GKLocalPlayer.localPlayer.authenticateHandler = ^(UIViewController* signIn, NSError* /*error*/) {
          if (signIn != nil)
          {
              present_when_possible(signIn, 5);
          }
          notify_change();
        };
    }

    ~IosGameCenter() override
    {
        GKLocalPlayer.localPlayer.authenticateHandler = nil;
    }

    [[nodiscard]] bool available() const override
    {
        return GKLocalPlayer.localPlayer.isAuthenticated;
    }

    void submit_score(std::string_view leaderboard_id, std::int64_t score) override
    {
        if (!available())
        {
            return;
        }
        [GKLeaderboard submitScore:static_cast<NSInteger>(score)
                           context:0
                            player:GKLocalPlayer.localPlayer
                    leaderboardIDs:@[ to_string(leaderboard_id) ]
                 completionHandler:^(NSError* /*error*/){
                     // A score that could not be sent is dropped: the leaderboard on the device has it.
                 }];
    }

    void show_leaderboard(std::string_view leaderboard_id) override
    {
        if (!available())
        {
            return;
        }
        // The delegate is held here: GameKit keeps only a weak reference to it.
        static SnakeGameCenterDelegate* const delegate = [[SnakeGameCenterDelegate alloc] init];
        GKGameCenterViewController* controller =
            [[GKGameCenterViewController alloc] initWithLeaderboardID:to_string(leaderboard_id)
                                                          playerScope:GKLeaderboardPlayerScopeGlobal
                                                            timeScope:GKLeaderboardTimeScopeAllTime];
        controller.gameCenterDelegate = delegate;
        present_when_possible(controller, 0);
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

// A phone is steered by touch and tilt. Gamepads on a phone or tablet are not read yet.
std::unique_ptr<GamepadSource> make_gamepad_source()
{
    return nullptr;
}

std::unique_ptr<Haptics> make_haptics()
{
    return std::make_unique<IosHaptics>();
}

std::unique_ptr<GameCenter> make_game_center()
{
    return std::make_unique<IosGameCenter>();
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
        // The phone may have been turned while the screen was held: ask the views to read the allowed orientations
        // again, so it catches up now.
        for (UIScene* scene in UIApplication.sharedApplication.connectedScenes)
        {
            if ([scene isKindOfClass:UIWindowScene.class])
            {
                for (UIWindow* window in static_cast<UIWindowScene*>(scene).windows)
                {
                    [window.rootViewController setNeedsUpdateOfSupportedInterfaceOrientations];
                }
            }
        }
    }
}

} // namespace snake::ui::device
