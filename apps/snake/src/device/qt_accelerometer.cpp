// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/qt_accelerometer.hpp"

#include "device/accelerometer.hpp"
#include "snake/input/sample_source.hpp"

#include <QAccelerometer>
#include <QAccelerometerReading>
#include <QObject>
#include <QSensor>
#include <cstddef>
#include <deque>
#include <memory>
#include <optional>

namespace snake::ui::device
{

namespace
{

/// How many readings are kept when nobody asks for them: a second or so at 60 a second. A program that stops reading
/// loses the oldest, not the newest.
constexpr std::size_t MAX_QUEUED = 64;

constexpr int DATA_RATE_HZ = 60;

class QtAccelerometerSource final : public QObject, public input::SampleSource
{
public:
    QtAccelerometerSource()
    {
        m_Sensor.setDataRate(DATA_RATE_HZ);
        connect(&m_Sensor, &QSensor::readingChanged, this, &QtAccelerometerSource::on_reading);
    }

    /// Connects to the sensor and starts it. False when the device has none, or it cannot be had.
    [[nodiscard]] bool start()
    {
        if (!m_Sensor.connectToBackend())
        {
            return false;
        }
        // The axes should be those of the screen, which may be on its side: x to the player's right and y up the screen
        // whichever way the phone is held. Qt can turn the readings so, but only a backend that supports it does
        // (QSensor::AxesOrientation); iOS's does not, and in the mode that it ignores it reads in the device's own
        // axes whichever way the screen is drawn. Then the readings are left as they are, and say so, and the input
        // controller turns them (input::ScreenFrameSource).
        m_DeviceFrame = !m_Sensor.isFeatureSupported(QSensor::AxesOrientation);
        if (!m_DeviceFrame)
        {
            m_Sensor.setAxesOrientationMode(QAccelerometer::AutomaticOrientation);
        }
        return m_Sensor.start();
    }

    [[nodiscard]] bool in_device_frame() const noexcept override
    {
        return m_DeviceFrame;
    }

    std::optional<input::AccelSample> next() noexcept override
    {
        if (m_Queue.empty())
        {
            return std::nullopt;
        }
        const input::AccelSample sample = m_Queue.front();
        m_Queue.pop_front();
        return sample;
    }

private:
    void on_reading()
    {
        const QAccelerometerReading* reading = m_Sensor.reading();
        if (reading == nullptr)
        {
            return;
        }
        m_Queue.push_back(accel_sample_from(reading->x(), reading->y(), reading->z(), reading->timestamp()));
        if (m_Queue.size() > MAX_QUEUED)
        {
            m_Queue.pop_front();
        }
    }

    QAccelerometer m_Sensor;
    std::deque<input::AccelSample> m_Queue;
    bool m_DeviceFrame = false;
};

} // namespace

std::unique_ptr<input::SampleSource> make_qt_accelerometer()
{
    auto source = std::make_unique<QtAccelerometerSource>();
    if (!source->start())
    {
        return nullptr;
    }
    return source;
}

} // namespace snake::ui::device
