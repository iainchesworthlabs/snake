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
        // The axes are those of the screen, which does not turn: the game is portrait and the activity or the
        // application is fixed to it, so the readings need no rotation.
        m_Sensor.setAxesOrientationMode(QAccelerometer::FixedOrientation);
        m_Sensor.setDataRate(DATA_RATE_HZ);
        connect(&m_Sensor, &QSensor::readingChanged, this, &QtAccelerometerSource::on_reading);
    }

    /// Connects to the sensor and starts it. False when the device has none, or it cannot be had.
    [[nodiscard]] bool start()
    {
        return m_Sensor.connectToBackend() && m_Sensor.start();
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
