#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "tools/simulator/view/gui/ScopeToolbar.hpp"
#include "ui/backend/qt/QtPaintedWidget.hpp"
#include "ui/scope/ScopeCore.hpp"
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

        ui::widgets::HexagonCore hexagonCore;
        ui::scope::ScopeCore currentScopeCore;
        ui::scope::ScopeCore voltageScopeCore;
        ui::scope::ScopeCore electricalRlsScopeCore;
        ui::scope::ScopeCore mechanicalRlsScopeCore;

        ui::backend::qt::QtPaintedWidget* hexagonWidget;
        ui::backend::qt::QtPaintedWidget* currentScope;
        ui::backend::qt::QtPaintedWidget* voltageScope;
        ui::backend::qt::QtPaintedWidget* electricalRlsScope;
        ui::backend::qt::QtPaintedWidget* mechanicalRlsScope;

        ScopeToolbar* currentScopeToolbar;
        ScopeToolbar* voltageScopeToolbar;

        QTimer refreshTimer;
    };
}
