#pragma once

#include "core/foc/interfaces/Units.hpp"
#include <QPointF>
#include <QTimer>
#include <QWidget>

namespace simulator
{
    class HexagonWidget
        : public QWidget
    {
        Q_OBJECT

    public:
        explicit HexagonWidget(QWidget* parent = nullptr);

        void SetDcLink(foc::Volts vdc);
        void SetSample(float va, float vb, float vc, float vAlpha, float vBeta);
        void Clear();

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        void DrawAxes(QPainter& painter, const QRectF& plotArea, float scale, QPointF center) const;
        void DrawHexagon(QPainter& painter, float scale, QPointF center) const;
        void DrawInscribedCircle(QPainter& painter, float scale, QPointF center) const;
        void DrawPhasePhasors(QPainter& painter, float scale, QPointF center) const;
        void DrawResultantVector(QPainter& painter, float scale, QPointF center) const;
        void DrawInfoOverlay(QPainter& painter, const QRectF& plotArea) const;

        int SectorOf(float vAlpha, float vBeta) const;

        float vdcVolts{ 24.0f };

        float vaSample{ 0.0f };
        float vbSample{ 0.0f };
        float vcSample{ 0.0f };
        float vAlphaSample{ 0.0f };
        float vBetaSample{ 0.0f };

        // Smoothed peak |V| used to auto-scale the view
        float peakMagnitude{ 0.0f };

        QTimer refreshTimer;
    };
}
