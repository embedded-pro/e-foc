#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "tools/simulator/view/gui/HexagonWidget.hpp"
#include "tools/simulator/view/gui/ScopeToolbar.hpp"
#include "tools/simulator/view/gui/ScopeWidget.hpp"
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
        void Clear();

    private:
        ScopeWidget* currentScope;
        ScopeToolbar* currentScopeToolbar;
        ScopeWidget* voltageScope;
        ScopeToolbar* voltageScopeToolbar;
        HexagonWidget* hexagonWidget;
    };
}
