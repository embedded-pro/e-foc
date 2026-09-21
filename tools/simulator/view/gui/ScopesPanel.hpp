#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "tools/simulator/view/gui/ScopeToolbar.hpp"
#include "tools/simulator/view/gui/ScopeWidget.hpp"
#include "ui/backend/qt/QtPaintedWidget.hpp"
#include "ui/widgets/HexagonCore.hpp"
#include <QLabel>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <span>

namespace simulator
{
    class ScopesPanel
        : public QWidget
    {
        Q_OBJECT

    public:
        explicit ScopesPanel(QWidget* parent = nullptr);

        void AddCurrentSample(std::span<const float> sample);
        void AddVoltageSample(std::span<const float> sample);
        void SetHexagonSample(float va, float vb, float vc, float vAlpha, float vBeta);
        void SetDcLink(foc::Volts vdc);
        void SetMode(const QString& label);
        void Clear();
        void AddElectricalRlsSample(float Rhat, float Lhat);
        void AddMechanicalRlsSample(float Bhat, float Jhat);

    private:
        QLabel* modeLabel;
        ScopeWidget* currentScope;
        ScopeToolbar* currentScopeToolbar;
        ScopeWidget* voltageScope;
        ScopeToolbar* voltageScopeToolbar;
        ui::widgets::HexagonCore hexagonCore;
        ui::backend::qt::QtPaintedWidget* hexagonWidget;
        QTimer hexagonRefreshTimer;
        ScopeWidget* electricalRlsScope{ nullptr };
        ScopeWidget* mechanicalRlsScope{ nullptr };
    };
}
