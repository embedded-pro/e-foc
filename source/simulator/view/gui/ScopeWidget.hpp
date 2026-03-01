#pragma once

#include <QTimer>
#include <QWidget>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace simulator
{
    class ScopeWidget
        : public QWidget
    {
        Q_OBJECT

    public:
        static constexpr std::size_t maxChannels = 4;

        enum class TriggerMode : uint8_t
        {
            Auto,
            Normal,
            Single
        };

        enum class TriggerEdge : uint8_t
        {
            Rising,
            Falling
        };

        struct ChannelConfig
        {
            std::string name;
            QColor color;
            bool enabled = true;
        };

        explicit ScopeWidget(QWidget* parent = nullptr);

        void SetChannelCount(std::size_t count);
        void SetChannelConfig(std::size_t channel, const ChannelConfig& config);
        void SetTimePerDivision(float secondsPerDiv);
        void SetTriggerChannel(std::size_t channel);
        void SetTriggerLevel(float level);
        void SetTriggerMode(TriggerMode mode);
        void SetTriggerEdge(TriggerEdge edge);
        void SetSamplePeriod(float seconds);

        void AddSample(std::span<const float> channelValues);
        void Clear();
        void SetRunning(bool running);
        void ForceTrigger();

        float TimePerDivision() const;
        float TriggerLevel() const;
        TriggerMode CurrentTriggerMode() const;
        TriggerEdge CurrentTriggerEdge() const;
        std::size_t TriggerChannel() const;
        bool IsRunning() const;

    signals:
        void triggerFired();

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        static constexpr std::size_t horizontalDivisions = 10;
        static constexpr std::size_t verticalDivisions = 8;
        static constexpr std::size_t ringBufferCapacity = 256 * 1024;

        struct RingBuffer
        {
            std::vector<float> data;
            std::size_t head = 0;
            std::size_t count = 0;

            void Resize(std::size_t capacity);
            void Push(float value);
            float At(std::size_t index) const;
        };

        void DrawGrid(QPainter& painter, const QRectF& plotArea) const;
        void DrawChannels(QPainter& painter, const QRectF& plotArea) const;
        void DrawTriggerLine(QPainter& painter, const QRectF& plotArea) const;
        void DrawLabels(QPainter& painter, const QRectF& plotArea) const;
        void DrawChannelFromTrigger(QPainter& painter, const QRectF& plotArea, std::size_t channel) const;
        void DrawChannelFreeRunning(QPainter& painter, const QRectF& plotArea, std::size_t channel) const;

        std::size_t FindTriggerPoint() const;
        std::pair<float, float> ComputeVerticalRange(std::size_t startSample, std::size_t sampleCount) const;

        std::size_t channelCount = 0;
        std::array<ChannelConfig, maxChannels> channelConfigs;
        std::array<RingBuffer, maxChannels> ringBuffers;

        float timePerDiv = 0.01f;
        float samplePeriod = 10e-6f;
        std::size_t triggerChannel = 0;
        float triggerLevel = 0.0f;
        TriggerMode triggerMode = TriggerMode::Auto;
        TriggerEdge triggerEdge = TriggerEdge::Rising;
        bool running = true;
        bool triggered = false;
        bool singleShotDone = false;

        std::size_t lastTriggerPoint = 0;
        float previousTriggerSample = 0.0f;

        QTimer refreshTimer;
    };
}
