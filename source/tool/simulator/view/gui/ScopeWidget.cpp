#include "source/tool/simulator/view/gui/ScopeWidget.hpp"
#include <QPainter>
#include <QPen>
#include <algorithm>
#include <cmath>

namespace simulator
{
    namespace
    {
        constexpr int plotMarginLeft = 60;
        constexpr int plotMarginRight = 20;
        constexpr int plotMarginTop = 10;
        constexpr int plotMarginBottom = 30;
        constexpr float gridLineAlpha = 0.3f;
        constexpr float triggerLineAlpha = 0.6f;
        constexpr int traceLineWidth = 2;
        constexpr int gridLineWidth = 1;
        constexpr float verticalMarginFraction = 0.1f;
        constexpr float fallbackVerticalRange = 1.0f;
        constexpr int labelFontSize = 8;
        constexpr float preTriggerFraction = 0.2f;
        constexpr int autoTriggerSearchLimit = 65536;

        QString FormatTimeDiv(float secondsPerDiv)
        {
            if (secondsPerDiv >= 1.0f)
                return QString("%1 s/div").arg(static_cast<double>(secondsPerDiv), 0, 'f', 1);
            if (secondsPerDiv >= 1e-3f)
                return QString("%1 ms/div").arg(static_cast<double>(secondsPerDiv * 1e3f), 0, 'f', 2);

            return QString("%1 µs/div").arg(static_cast<double>(secondsPerDiv * 1e6f), 0, 'f', 0);
        }

        QString FormatValue(float value)
        {
            if (std::abs(value) >= 1.0f)
                return QString::number(static_cast<double>(value), 'f', 2);
            if (std::abs(value) >= 1e-3f)
                return QString("%1m").arg(static_cast<double>(value * 1e3f), 0, 'f', 1);

            return QString("%1µ").arg(static_cast<double>(value * 1e6f), 0, 'f', 0);
        }
    }

    // --- RingBuffer ---

    void ScopeWidget::RingBuffer::Resize(std::size_t capacity)
    {
        data.resize(capacity, 0.0f);
        head = 0;
        count = 0;
    }

    void ScopeWidget::RingBuffer::Push(float value)
    {
        data[head] = value;
        head = (head + 1) % data.size();

        if (count < data.size())
            ++count;
    }

    float ScopeWidget::RingBuffer::At(std::size_t index) const
    {
        if (count < data.size())
            return data[index];

        return data[(head + index) % data.size()];
    }

    // --- ScopeWidget ---

    ScopeWidget::ScopeWidget(QWidget* parent)
        : QWidget(parent)
    {
        setMinimumHeight(250);
        setStyleSheet("background-color: #0a0a0a;");

        for (auto& buf : ringBuffers)
            buf.Resize(ringBufferCapacity);

        connect(&refreshTimer, &QTimer::timeout, this, QOverload<>::of(&QWidget::update));
        refreshTimer.start(33);
    }

    void ScopeWidget::SetChannelCount(std::size_t count)
    {
        channelCount = std::min(count, maxChannels);
    }

    void ScopeWidget::SetChannelConfig(std::size_t channel, const ChannelConfig& config)
    {
        if (channel < maxChannels)
            channelConfigs[channel] = config;
    }

    void ScopeWidget::SetTimePerDivision(float secondsPerDiv)
    {
        timePerDiv = secondsPerDiv;
        update();
    }

    void ScopeWidget::SetTriggerChannel(std::size_t channel)
    {
        triggerChannel = std::min(channel, channelCount > 0 ? channelCount - 1 : 0u);
    }

    void ScopeWidget::SetTriggerLevel(float level)
    {
        triggerLevel = level;
        update();
    }

    void ScopeWidget::SetTriggerMode(TriggerMode mode)
    {
        triggerMode = mode;
        singleShotDone = false;
        update();
    }

    void ScopeWidget::SetTriggerEdge(TriggerEdge edge)
    {
        triggerEdge = edge;
    }

    void ScopeWidget::SetSamplePeriod(float seconds)
    {
        samplePeriod = seconds;
    }

    void ScopeWidget::AddSample(std::span<const float> channelValues)
    {
        if (!running)
            return;

        if (triggerMode == TriggerMode::Single && singleShotDone)
            return;

        for (std::size_t i = 0; i < std::min(channelValues.size(), channelCount); ++i)
            ringBuffers[i].Push(channelValues[i]);

        if (triggerChannel < channelCount && ringBuffers[triggerChannel].count > 1)
        {
            float current = channelValues[triggerChannel];
            bool edgeDetected = false;

            if (triggerEdge == TriggerEdge::Rising)
                edgeDetected = (previousTriggerSample < triggerLevel && current >= triggerLevel);
            else
                edgeDetected = (previousTriggerSample > triggerLevel && current <= triggerLevel);

            if (edgeDetected)
            {
                triggered = true;
                lastTriggerPoint = ringBuffers[triggerChannel].count > 0 ? ringBuffers[triggerChannel].count - 1 : 0;

                if (triggerMode == TriggerMode::Single)
                    singleShotDone = true;

                emit triggerFired();
            }

            previousTriggerSample = current;
        }
    }

    void ScopeWidget::Clear()
    {
        for (std::size_t i = 0; i < maxChannels; ++i)
        {
            std::ranges::fill(ringBuffers[i].data, 0.0f);
            ringBuffers[i].head = 0;
            ringBuffers[i].count = 0;
        }

        triggered = false;
        singleShotDone = false;
        lastTriggerPoint = 0;
        previousTriggerSample = 0.0f;
        update();
    }

    void ScopeWidget::SetRunning(bool run)
    {
        running = run;

        if (running)
            singleShotDone = false;
    }

    void ScopeWidget::ForceTrigger()
    {
        triggered = true;
        lastTriggerPoint = ringBuffers[triggerChannel].count > 0 ? ringBuffers[triggerChannel].count - 1 : 0;
        singleShotDone = false;
        update();
    }

    float ScopeWidget::TimePerDivision() const
    {
        return timePerDiv;
    }

    float ScopeWidget::TriggerLevel() const
    {
        return triggerLevel;
    }

    ScopeWidget::TriggerMode ScopeWidget::CurrentTriggerMode() const
    {
        return triggerMode;
    }

    ScopeWidget::TriggerEdge ScopeWidget::CurrentTriggerEdge() const
    {
        return triggerEdge;
    }

    std::size_t ScopeWidget::TriggerChannel() const
    {
        return triggerChannel;
    }

    bool ScopeWidget::IsRunning() const
    {
        return running;
    }

    void ScopeWidget::paintEvent(QPaintEvent* /*event*/)
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QRectF plotArea(plotMarginLeft, plotMarginTop,
            width() - plotMarginLeft - plotMarginRight,
            height() - plotMarginTop - plotMarginBottom);

        DrawGrid(painter, plotArea);
        DrawTriggerLine(painter, plotArea);
        DrawChannels(painter, plotArea);
        DrawLabels(painter, plotArea);
    }

    void ScopeWidget::DrawGrid(QPainter& painter, const QRectF& plotArea) const
    {
        QPen gridPen(QColor(80, 80, 80, static_cast<int>(gridLineAlpha * 255)));
        gridPen.setWidth(gridLineWidth);
        gridPen.setStyle(Qt::DotLine);
        painter.setPen(gridPen);

        for (std::size_t i = 0; i <= horizontalDivisions; ++i)
        {
            auto x = plotArea.left() + plotArea.width() * static_cast<double>(i) / static_cast<double>(horizontalDivisions);
            painter.drawLine(QPointF(x, plotArea.top()), QPointF(x, plotArea.bottom()));
        }

        for (std::size_t i = 0; i <= verticalDivisions; ++i)
        {
            auto y = plotArea.top() + plotArea.height() * static_cast<double>(i) / static_cast<double>(verticalDivisions);
            painter.drawLine(QPointF(plotArea.left(), y), QPointF(plotArea.right(), y));
        }

        // Center crosshair (brighter)
        QPen centerPen(QColor(120, 120, 120, static_cast<int>(gridLineAlpha * 255 * 1.5)));
        centerPen.setWidth(gridLineWidth);
        painter.setPen(centerPen);

        auto centerX = plotArea.left() + plotArea.width() / 2.0;
        auto centerY = plotArea.top() + plotArea.height() / 2.0;
        painter.drawLine(QPointF(centerX, plotArea.top()), QPointF(centerX, plotArea.bottom()));
        painter.drawLine(QPointF(plotArea.left(), centerY), QPointF(plotArea.right(), centerY));

        // Border
        QPen borderPen(QColor(100, 100, 100));
        borderPen.setWidth(gridLineWidth);
        painter.setPen(borderPen);
        painter.drawRect(plotArea);
    }

    void ScopeWidget::DrawTriggerLine(QPainter& painter, const QRectF& plotArea) const
    {
        if (channelCount == 0)
            return;

        auto totalSamples = static_cast<std::size_t>(timePerDiv * static_cast<float>(horizontalDivisions) / samplePeriod);
        auto [vMin, vMax] = ComputeVerticalRange(0, std::min(totalSamples, ringBuffers[0].count));

        if (triggerLevel < vMin || triggerLevel > vMax)
            return;

        auto normalizedY = 1.0 - static_cast<double>(triggerLevel - vMin) / static_cast<double>(vMax - vMin);
        auto y = plotArea.top() + plotArea.height() * normalizedY;

        QPen trigPen(QColor(255, 100, 0, static_cast<int>(triggerLineAlpha * 255)));
        trigPen.setWidth(gridLineWidth);
        trigPen.setStyle(Qt::DashLine);
        painter.setPen(trigPen);
        painter.drawLine(QPointF(plotArea.left(), y), QPointF(plotArea.right(), y));

        // Trigger marker arrow on left
        painter.setBrush(QColor(255, 100, 0));
        constexpr int arrowSize = 6;
        QPolygonF arrow;
        arrow << QPointF(plotArea.left() - arrowSize * 2, y)
              << QPointF(plotArea.left() - arrowSize, y - arrowSize)
              << QPointF(plotArea.left() - arrowSize, y + arrowSize);
        painter.drawPolygon(arrow);
    }

    void ScopeWidget::DrawChannels(QPainter& painter, const QRectF& plotArea) const
    {
        for (std::size_t ch = 0; ch < channelCount; ++ch)
        {
            if (!channelConfigs[ch].enabled)
                continue;

            if (triggered && triggerMode != TriggerMode::Auto)
                DrawChannelFromTrigger(painter, plotArea, ch);
            else
                DrawChannelFreeRunning(painter, plotArea, ch);
        }
    }

    std::size_t ScopeWidget::FindTriggerPoint() const
    {
        if (triggerChannel >= channelCount)
            return 0;

        const auto& buf = ringBuffers[triggerChannel];
        if (buf.count < 2)
            return 0;

        auto searchStart = buf.count > static_cast<std::size_t>(autoTriggerSearchLimit)
                               ? buf.count - static_cast<std::size_t>(autoTriggerSearchLimit)
                               : std::size_t{ 0 };

        std::size_t bestTrigger = buf.count - 1;

        for (auto i = buf.count - 1; i > searchStart; --i)
        {
            auto prev = buf.At(i - 1);
            auto curr = buf.At(i);

            if (triggerEdge == TriggerEdge::Rising)
            {
                if (prev < triggerLevel && curr >= triggerLevel)
                    return i;
            }
            else
            {
                if (prev > triggerLevel && curr <= triggerLevel)
                    return i;
            }
        }

        return bestTrigger;
    }

    void ScopeWidget::DrawChannelFromTrigger(QPainter& painter, const QRectF& plotArea, std::size_t channel) const
    {
        const auto& buf = ringBuffers[channel];
        auto totalSamples = static_cast<std::size_t>(timePerDiv * static_cast<float>(horizontalDivisions) / samplePeriod);

        if (totalSamples < 2 || buf.count < 2)
            return;

        auto preTriggerSamples = static_cast<std::size_t>(static_cast<float>(totalSamples) * preTriggerFraction);
        auto trigPoint = FindTriggerPoint();

        std::size_t startSample = 0;
        if (trigPoint > preTriggerSamples)
            startSample = trigPoint - preTriggerSamples;

        auto available = buf.count - startSample;
        auto samplesToShow = std::min(totalSamples, available);

        auto [vMin, vMax] = ComputeVerticalRange(startSample, samplesToShow);
        auto vRange = vMax - vMin;

        if (vRange < 1e-12f)
        {
            vMin -= fallbackVerticalRange / 2.0f;
            vMax += fallbackVerticalRange / 2.0f;
            vRange = vMax - vMin;
        }

        QPen pen(channelConfigs[channel].color);
        pen.setWidth(traceLineWidth);
        painter.setPen(pen);

        QPointF prevPoint;
        for (std::size_t i = 0; i < samplesToShow; ++i)
        {
            float value = buf.At(startSample + i);
            auto x = plotArea.left() + plotArea.width() * static_cast<double>(i) / static_cast<double>(totalSamples);
            auto normalizedY = 1.0 - static_cast<double>(value - vMin) / static_cast<double>(vRange);
            auto y = plotArea.top() + plotArea.height() * normalizedY;

            QPointF currentPoint(x, y);
            if (i > 0)
                painter.drawLine(prevPoint, currentPoint);
            prevPoint = currentPoint;
        }
    }

    void ScopeWidget::DrawChannelFreeRunning(QPainter& painter, const QRectF& plotArea, std::size_t channel) const
    {
        const auto& buf = ringBuffers[channel];
        auto totalSamples = static_cast<std::size_t>(timePerDiv * static_cast<float>(horizontalDivisions) / samplePeriod);

        if (totalSamples < 2 || buf.count < 2)
            return;

        auto samplesToShow = std::min(totalSamples, buf.count);
        auto startSample = buf.count - samplesToShow;

        auto [vMin, vMax] = ComputeVerticalRange(startSample, samplesToShow);
        auto vRange = vMax - vMin;

        if (vRange < 1e-12f)
        {
            vMin -= fallbackVerticalRange / 2.0f;
            vMax += fallbackVerticalRange / 2.0f;
            vRange = vMax - vMin;
        }

        QPen pen(channelConfigs[channel].color);
        pen.setWidth(traceLineWidth);
        painter.setPen(pen);

        QPointF prevPoint;
        for (std::size_t i = 0; i < samplesToShow; ++i)
        {
            float value = buf.At(startSample + i);
            auto x = plotArea.left() + plotArea.width() * static_cast<double>(i) / static_cast<double>(totalSamples);
            auto normalizedY = 1.0 - static_cast<double>(value - vMin) / static_cast<double>(vRange);
            auto y = plotArea.top() + plotArea.height() * normalizedY;

            QPointF currentPoint(x, y);
            if (i > 0)
                painter.drawLine(prevPoint, currentPoint);
            prevPoint = currentPoint;
        }
    }

    std::pair<float, float> ScopeWidget::ComputeVerticalRange(std::size_t startSample, std::size_t sampleCount) const
    {
        float globalMin = std::numeric_limits<float>::max();
        float globalMax = std::numeric_limits<float>::lowest();

        for (std::size_t ch = 0; ch < channelCount; ++ch)
        {
            if (!channelConfigs[ch].enabled)
                continue;

            const auto& buf = ringBuffers[ch];
            auto end = std::min(startSample + sampleCount, buf.count);

            for (auto i = startSample; i < end; ++i)
            {
                float v = buf.At(i);
                globalMin = std::min(globalMin, v);
                globalMax = std::max(globalMax, v);
            }
        }

        if (globalMin > globalMax)
        {
            globalMin = -fallbackVerticalRange;
            globalMax = fallbackVerticalRange;
        }

        auto range = globalMax - globalMin;
        auto margin = range * verticalMarginFraction;

        if (margin < 1e-9f)
            margin = fallbackVerticalRange * verticalMarginFraction;

        return { globalMin - margin, globalMax + margin };
    }

    void ScopeWidget::DrawLabels(QPainter& painter, const QRectF& plotArea) const
    {
        QFont labelFont;
        labelFont.setPixelSize(labelFontSize);
        painter.setFont(labelFont);

        // Time axis labels
        painter.setPen(QColor(180, 180, 180));
        auto totalTime = timePerDiv * static_cast<float>(horizontalDivisions);

        for (std::size_t i = 0; i <= horizontalDivisions; i += 2)
        {
            auto x = plotArea.left() + plotArea.width() * static_cast<double>(i) / static_cast<double>(horizontalDivisions);
            auto t = totalTime * static_cast<float>(i) / static_cast<float>(horizontalDivisions);

            if (t >= 1.0f)
                painter.drawText(QPointF(x - 15, plotArea.bottom() + 15), QString("%1s").arg(static_cast<double>(t), 0, 'f', 2));
            else if (t >= 1e-3f)
                painter.drawText(QPointF(x - 15, plotArea.bottom() + 15), QString("%1ms").arg(static_cast<double>(t * 1e3f), 0, 'f', 1));
            else
                painter.drawText(QPointF(x - 15, plotArea.bottom() + 15), QString("%1µs").arg(static_cast<double>(t * 1e6f), 0, 'f', 0));
        }

        // Vertical axis labels
        auto totalSamples = static_cast<std::size_t>(timePerDiv * static_cast<float>(horizontalDivisions) / samplePeriod);
        auto samplesToShow = std::min(totalSamples, ringBuffers[0].count);

        std::size_t startSample = 0;
        if (triggered && triggerMode != TriggerMode::Auto)
        {
            auto preTriggerSamples = static_cast<std::size_t>(static_cast<float>(totalSamples) * preTriggerFraction);
            auto trigPoint = FindTriggerPoint();

            if (trigPoint > preTriggerSamples)
                startSample = trigPoint - preTriggerSamples;
        }
        else if (ringBuffers[0].count > samplesToShow)
        {
            startSample = ringBuffers[0].count - samplesToShow;
        }

        auto [vMin, vMax] = ComputeVerticalRange(startSample, samplesToShow);

        for (std::size_t i = 0; i <= verticalDivisions; i += 2)
        {
            auto y = plotArea.top() + plotArea.height() * static_cast<double>(i) / static_cast<double>(verticalDivisions);
            auto value = vMax - (vMax - vMin) * static_cast<float>(i) / static_cast<float>(verticalDivisions);
            painter.drawText(QPointF(plotArea.left() - 55, y + 4), FormatValue(value));
        }

        // Time/div and trigger info in top-right corner
        painter.setPen(QColor(200, 200, 200));
        auto timeDivStr = FormatTimeDiv(timePerDiv);
        painter.drawText(QPointF(plotArea.right() - 120, plotArea.top() + 15), timeDivStr);

        // Trigger indicator
        QString trigModeStr;
        switch (triggerMode)
        {
            case TriggerMode::Auto:
                trigModeStr = "AUTO";
                break;
            case TriggerMode::Normal:
                trigModeStr = "NORM";
                break;
            case TriggerMode::Single:
                trigModeStr = "SINGLE";
                break;
        }

        auto trigColor = triggered ? QColor(0, 200, 0) : QColor(200, 200, 0);
        painter.setPen(trigColor);
        painter.drawText(QPointF(plotArea.right() - 120, plotArea.top() + 30), QString("Trig: %1").arg(trigModeStr));

        // Channel legend
        auto legendX = plotArea.left() + 5;
        auto legendY = plotArea.top() + 15;

        for (std::size_t ch = 0; ch < channelCount; ++ch)
        {
            if (!channelConfigs[ch].enabled)
                continue;

            painter.setPen(channelConfigs[ch].color);
            painter.drawText(QPointF(legendX, legendY),
                QString::fromStdString(channelConfigs[ch].name));
            legendX += 60;
        }
    }
}
