#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "ui/backend/qt/QtFormView.hpp"
#include "ui/backend/qt/QtPaintedWidget.hpp"
#include "ui/scope/ScopeController.hpp"
#include "ui/scope/ScopeControls.hpp"
#include "ui/scope/ScopeCore.hpp"
#include "ui/widgets/HexagonCore.hpp"
#include <QLabel>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <memory>
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

        ui::scope::ScopeControls currentScopeControls{ 3 };
        ui::scope::ScopeControls voltageScopeControls{ 3 };

        ui::backend::qt::QtFormView* currentScopeForm;
        ui::backend::qt::QtFormView* voltageScopeForm;

        std::unique_ptr<ui::scope::ScopeController> currentScopeController;
        std::unique_ptr<ui::scope::ScopeController> voltageScopeController;

        QTimer refreshTimer;
    };
}
