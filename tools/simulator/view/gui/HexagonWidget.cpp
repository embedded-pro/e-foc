#include "tools/simulator/view/gui/HexagonWidget.hpp"
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QRectF>
#include <QSizePolicy>
#include <QString>
#include <QStringList>
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace simulator
{
    namespace
    {
        constexpr int plotMargin = 40;
        constexpr int axisLabelFontSize = 9;
        constexpr int infoFontSize = 10;
        constexpr float twoThirds = 2.0f / 3.0f;
        constexpr float invSqrt3 = std::numbers::inv_sqrt3_v<float>;
        constexpr float pi = std::numbers::pi_v<float>;
        constexpr float pi_over_3 = pi / 3.0f;
        constexpr float twoPi_over_3 = 2.0f * pi / 3.0f;
        constexpr int refreshIntervalMs = 33;

        constexpr std::array<float, 3> phaseAngles = { 0.0f, twoPi_over_3, -twoPi_over_3 };

        QPointF VertexFor(std::size_t k, float radiusPixels)
        {
            const auto angle = static_cast<float>(k) * pi_over_3;
            return { radiusPixels * std::cos(angle), -radiusPixels * std::sin(angle) };
        }

        QPointF ToScreen(QPointF center, float vx, float vy, float scale)
        {
            return { center.x() + vx * scale, center.y() - vy * scale };
        }
    }

    HexagonWidget::HexagonWidget(QWidget* parent)
        : QWidget(parent)
    {
        setMinimumHeight(520);
        setMinimumWidth(520);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setStyleSheet("background-color: #0a0a0a;");

        connect(&refreshTimer, &QTimer::timeout, this, QOverload<>::of(&QWidget::update));
        refreshTimer.start(refreshIntervalMs);
    }

    void HexagonWidget::SetDcLink(foc::Volts vdc)
    {
        vdcVolts = vdc.Value();
        update();
    }

    void HexagonWidget::SetSample(float va, float vb, float vc, float vAlpha, float vBeta)
    {
        vaSample = va;
        vbSample = vb;
        vcSample = vc;
        vAlphaSample = vAlpha;
        vBetaSample = vBeta;

        // Track a smoothed peak magnitude for auto-scaling.
        // Fast attack so big jumps zoom out quickly; slow decay so the
        // view does not pump on every cycle.
        const auto magnitude = std::hypot(vAlpha, vBeta);
        constexpr float attack = 0.30f;
        constexpr float decay = 0.0015f;
        if (magnitude > peakMagnitude)
            peakMagnitude += attack * (magnitude - peakMagnitude);
        else
            peakMagnitude += decay * (magnitude - peakMagnitude);
    }

    void HexagonWidget::Clear()
    {
        vaSample = 0.0f;
        vbSample = 0.0f;
        vcSample = 0.0f;
        vAlphaSample = 0.0f;
        vBetaSample = 0.0f;
        peakMagnitude = 0.0f;
        update();
    }

    int HexagonWidget::SectorOf(float vAlpha, float vBeta) const
    {
        if (std::hypot(vAlpha, vBeta) < 1e-6f)
            return 0;

        auto angle = std::atan2(vBeta, vAlpha);
        if (angle < 0.0f)
            angle += 2.0f * pi;

        return static_cast<int>(angle / pi_over_3) + 1;
    }

    void HexagonWidget::paintEvent(QPaintEvent* /*event*/)
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const auto side = std::min(width(), height()) - 2 * plotMargin;
        if (side <= 0)
            return;

        const QRectF plotArea(
            (width() - side) / 2.0,
            (height() - side) / 2.0,
            side,
            side);

        const QPointF center = plotArea.center();

        // Fixed view: always show the full modulation hexagon (radius 2/3 Vdc).
        const float axisHalfRange = vdcVolts > 0.0f ? (twoThirds * vdcVolts) : 1.0f;
        const auto scale = static_cast<float>(side) / (2.0f * axisHalfRange);

        DrawAxes(painter, plotArea, scale, center);
        DrawHexagon(painter, scale, center);
        DrawInscribedCircle(painter, scale, center);
        DrawPhasePhasors(painter, scale, center);
        DrawResultantVector(painter, scale, center);
        DrawInfoOverlay(painter, plotArea);
    }

    void HexagonWidget::DrawAxes(QPainter& painter, const QRectF& plotArea, float scale, QPointF center) const
    {
        const float axisHalfRange = (static_cast<float>(plotArea.width()) / 2.0f) / scale;

        // Aim for ~6 ticks per side; pick a 1/2/5 * 10^k step in volts
        const float rawStep = axisHalfRange / 6.0f;
        const float pow10 = std::pow(10.0f, std::floor(std::log10(rawStep)));
        const float normalized = rawStep / pow10;
        float niceMantissa = 1.0f;
        if (normalized >= 5.0f)
            niceMantissa = 5.0f;
        else if (normalized >= 2.0f)
            niceMantissa = 2.0f;
        const float stepVolts = niceMantissa * pow10;

        QPen gridPen(QColor(50, 50, 50));
        gridPen.setStyle(Qt::DotLine);
        gridPen.setWidth(1);

        QFont labelFont = painter.font();
        labelFont.setPointSize(axisLabelFontSize);
        painter.setFont(labelFont);

        const int firstTick = static_cast<int>(std::floor(-axisHalfRange / stepVolts)) + 1;
        const int lastTick = static_cast<int>(std::floor(axisHalfRange / stepVolts));

        const int decimals = stepVolts >= 1.0f ? 0 : (stepVolts >= 0.1f ? 1 : 2);

        for (int k = firstTick; k <= lastTick; ++k)
        {
            if (k == 0)
                continue;
            const float v = k * stepVolts;
            const auto x = center.x() + v * scale;
            const auto y = center.y() - v * scale;

            painter.setPen(gridPen);
            painter.drawLine(QPointF{ x, plotArea.top() }, QPointF{ x, plotArea.bottom() });
            painter.drawLine(QPointF{ plotArea.left(), y }, QPointF{ plotArea.right(), y });

            painter.setPen(QColor(170, 170, 170));
            const QString label = QString::number(static_cast<double>(v), 'f', decimals);
            painter.drawText(QPointF{ x - 12, plotArea.bottom() + 14 }, label);
            painter.drawText(QPointF{ plotArea.left() - 34, y + 4 }, label);
        }

        QPen axisPen(QColor(180, 180, 180));
        axisPen.setWidth(1);
        painter.setPen(axisPen);
        painter.drawLine(QPointF{ plotArea.left(), center.y() }, QPointF{ plotArea.right(), center.y() });
        painter.drawLine(QPointF{ center.x(), plotArea.top() }, QPointF{ center.x(), plotArea.bottom() });

        painter.setPen(QColor(220, 220, 220));
        painter.drawText(QPointF{ center.x() + 6, plotArea.top() - 8 }, "β (V)");
        painter.drawText(QPointF{ plotArea.right() + 6, center.y() + 4 }, "α (V)");
    }

    void HexagonWidget::DrawHexagon(QPainter& painter, float scale, QPointF center) const
    {
        const auto radius = twoThirds * vdcVolts * scale;

        QPolygonF hexagon;
        for (std::size_t k = 0; k < 6; ++k)
            hexagon << (center + VertexFor(k, radius));

        QPen hexPen(QColor(160, 160, 160));
        hexPen.setWidth(1);
        painter.setPen(hexPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPolygon(hexagon);

        QPen sectorPen(QColor(90, 90, 90));
        sectorPen.setStyle(Qt::DashLine);
        sectorPen.setWidth(1);
        painter.setPen(sectorPen);
        for (std::size_t k = 0; k < 6; ++k)
            painter.drawLine(center, center + VertexFor(k, radius));

        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(QColor(240, 200, 60)));
        for (std::size_t k = 0; k < 6; ++k)
            painter.drawEllipse(center + VertexFor(k, radius), 5.0, 5.0);
    }

    void HexagonWidget::DrawInscribedCircle(QPainter& painter, float scale, QPointF center) const
    {
        const auto radius = invSqrt3 * vdcVolts * scale;

        QPen circlePen(QColor(60, 150, 230));
        circlePen.setWidth(2);
        painter.setPen(circlePen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(center, radius, radius);
    }

    void HexagonWidget::DrawPhasePhasors(QPainter& painter, float scale, QPointF center) const
    {
        const float circleRadius = vdcVolts > 0.0f ? (invSqrt3 * vdcVolts) : 1.0f;

        const std::array<float, 3> magnitudes = { vaSample, vbSample, vcSample };
        float peak = std::max({ std::abs(magnitudes[0]), std::abs(magnitudes[1]), std::abs(magnitudes[2]), 1e-6f });

        const std::array<QColor, 3> colors = {
            QColor(230, 90, 60),
            QColor(120, 200, 90),
            QColor(230, 160, 60)
        };

        for (std::size_t i = 0; i < 3; ++i)
        {
            // Normalize so the largest phase reaches the inscribed circle.
            const float normalized = (magnitudes[i] / peak) * circleRadius;
            const auto angle = phaseAngles[i];
            const auto vx = normalized * std::cos(angle);
            const auto vy = normalized * std::sin(angle);
            const auto tip = ToScreen(center, vx, vy, scale);

            QPen phasorPen(colors[i]);
            phasorPen.setWidth(2);
            painter.setPen(phasorPen);
            painter.drawLine(center, tip);

            painter.setPen(Qt::NoPen);
            painter.setBrush(QBrush(QColor(180, 130, 200)));
            painter.drawEllipse(tip, 4.0, 4.0);
        }
    }

    void HexagonWidget::DrawResultantVector(QPainter& painter, float scale, QPointF center) const
    {
        const auto magnitude = std::hypot(vAlphaSample, vBetaSample);
        if (magnitude < 1e-9f)
            return;

        // Always draw the resultant touching the inscribed circle so its
        // direction is visible regardless of the operating point. The true
        // magnitude is reported in the info overlay.
        const float circleRadius = vdcVolts > 0.0f ? (invSqrt3 * vdcVolts) : 1.0f;
        const float scaleFactor = circleRadius / magnitude;
        const auto displayAlpha = vAlphaSample * scaleFactor;
        const auto displayBeta = vBetaSample * scaleFactor;
        const auto tip = ToScreen(center, displayAlpha, displayBeta, scale);

        QPen vectorPen(QColor(255, 220, 60));
        vectorPen.setWidth(3);
        painter.setPen(vectorPen);
        painter.drawLine(center, tip);

        // Arrowhead
        const auto angle = std::atan2(vBetaSample, vAlphaSample);
        const float headLen = 14.0f;
        const float headHalfAngle = 0.45f;
        const QPointF wingA{
            tip.x() - headLen * std::cos(angle - headHalfAngle),
            tip.y() + headLen * std::sin(angle - headHalfAngle)
        };
        const QPointF wingB{
            tip.x() - headLen * std::cos(angle + headHalfAngle),
            tip.y() + headLen * std::sin(angle + headHalfAngle)
        };
        QPolygonF arrow;
        arrow << tip << wingA << wingB;
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(QColor(255, 220, 60)));
        painter.drawPolygon(arrow);
    }

    void HexagonWidget::DrawInfoOverlay(QPainter& painter, const QRectF& plotArea) const
    {
        QFont infoFont = painter.font();
        infoFont.setPointSize(infoFontSize);
        infoFont.setStyleHint(QFont::Monospace);
        infoFont.setFamily("monospace");
        painter.setFont(infoFont);

        const auto magnitude = std::hypot(vAlphaSample, vBetaSample);
        const auto sector = SectorOf(vAlphaSample, vBetaSample);

        const QString sectorText = sector == 0 ? QString("-") : QString::number(sector);
        const QStringList lines = {
            QString("\u03b1   = %1 V").arg(vAlphaSample, 7, 'f', 3),
            QString("\u03b2   = %1 V").arg(vBetaSample, 7, 'f', 3),
            QString("|V| = %1 V").arg(magnitude, 7, 'f', 3),
            QString("sector: %1").arg(sectorText)
        };

        const QFontMetrics metrics(infoFont);
        int boxWidth = 0;
        for (const auto& line : lines)
            boxWidth = std::max(boxWidth, metrics.horizontalAdvance(line));

        const int lineHeight = metrics.height();
        const int padding = 6;
        const QRectF box(
            plotArea.right() - boxWidth - 2 * padding,
            plotArea.top() + padding,
            boxWidth + 2 * padding,
            lineHeight * lines.size() + 2 * padding);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(QColor(20, 20, 20, 200)));
        painter.drawRect(box);

        painter.setPen(QColor(220, 220, 220));
        for (int i = 0; i < lines.size(); ++i)
            painter.drawText(QPointF{ box.left() + padding, box.top() + padding + (i + 1) * lineHeight - 4 }, lines[i]);
    }
}
