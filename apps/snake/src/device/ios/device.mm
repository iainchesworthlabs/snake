// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/device.hpp"

#include "device/haptics.hpp"
#include "device/qt_accelerometer.hpp"
#include "snake/input/sample_source.hpp"

#import <UIKit/UIKit.h>

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

} // namespace

std::unique_ptr<input::SampleSource> make_accelerometer()
{
    return make_qt_accelerometer();
}

std::unique_ptr<Haptics> make_haptics()
{
    return std::make_unique<IosHaptics>();
}

} // namespace snake::ui::device
