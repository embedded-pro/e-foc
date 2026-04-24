#include "tools/simulator/view/gui/ScopesPanel.hpp"
#include <QColor>
#include <QGroupBox>
#include <QVBoxLayout>
#include <memory>

namespace simulator
{
    namespace
    {
        template<typename T, typename... Args>
        T* QtOwned(Args&&... args)
        {
            return std::make_unique<T>(std::forward<Args>(args)...).release();
        }
    }

    ScopesPanel::ScopesPanel(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = QtOwned<QVBoxLayout>(this);

        // SVPWM hexagon (top, largest)
        auto* hexagonGroup = QtOwned<QGroupBox>("SVPWM Hexagon (Vβ vs Vα)", this);
        auto* hexagonLayout = QtOwned<QVBoxLayout>();

        hexagonWidget = QtOwned<HexagonWidget>(this);
        hexagonLayout->addWidget(hexagonWidget);

        hexagonGroup->setLayout(hexagonLayout);
        layout->addWidget(hexagonGroup, 3);

        // Phase currents scope
        auto* currentGroup = QtOwned<QGroupBox>("Phase Currents (A, B, C)", this);
        auto* currentLayout = QtOwned<QVBoxLayout>();

        currentScope = QtOwned<ScopeWidget>(this);
        currentScope->SetChannelCount(3);
        currentScope->SetChannelConfig(0, { "Ia", QColor(0, 150, 255) });
        currentScope->SetChannelConfig(1, { "Ib", QColor(255, 165, 0) });
        currentScope->SetChannelConfig(2, { "Ic", QColor(0, 200, 80) });

        currentScopeToolbar = QtOwned<ScopeToolbar>(*currentScope, this);

        currentLayout->addWidget(currentScopeToolbar);
        currentLayout->addWidget(currentScope);

        currentGroup->setLayout(currentLayout);
        layout->addWidget(currentGroup, 1);

        // Phase voltages scope
        auto* voltageGroup = QtOwned<QGroupBox>("Phase Voltages — inverter midpoint (Va, Vb, Vc)", this);
        auto* voltageLayout = QtOwned<QVBoxLayout>();

        voltageScope = QtOwned<ScopeWidget>(this);
        voltageScope->SetChannelCount(3);
        voltageScope->SetChannelConfig(0, { "Va", QColor(0, 150, 255) });
        voltageScope->SetChannelConfig(1, { "Vb", QColor(255, 165, 0) });
        voltageScope->SetChannelConfig(2, { "Vc", QColor(0, 200, 80) });

        voltageScopeToolbar = QtOwned<ScopeToolbar>(*voltageScope, this);

        voltageLayout->addWidget(voltageScopeToolbar);
        voltageLayout->addWidget(voltageScope);

        voltageGroup->setLayout(voltageLayout);
        layout->addWidget(voltageGroup, 1);
    }

    void ScopesPanel::AddCurrentSample(std::span<const float> sample)
    {
        currentScope->AddSample(sample);
    }

    void ScopesPanel::AddVoltageSample(std::span<const float> sample)
    {
        voltageScope->AddSample(sample);
    }

    void ScopesPanel::SetHexagonSample(float va, float vb, float vc, float vAlpha, float vBeta)
    {
        hexagonWidget->SetSample(va, vb, vc, vAlpha, vBeta);
    }

    void ScopesPanel::SetDcLink(foc::Volts vdc)
    {
        hexagonWidget->SetDcLink(vdc);
    }

    void ScopesPanel::Clear()
    {
        currentScope->Clear();
        voltageScope->Clear();
        hexagonWidget->Clear();
    }
}
