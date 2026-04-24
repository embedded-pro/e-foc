#include "tools/simulator/view/gui/ScopesPanel.hpp"
#include <QColor>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>
#include <memory>

namespace simulator
{
    namespace
    {
        constexpr int hexagonMaxHeight = 460;

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

        // Mode label at the top
        modeLabel = QtOwned<QLabel>("Idle", this);
        modeLabel->setAlignment(Qt::AlignCenter);
        QFont modeFont;
        modeFont.setBold(true);
        modeFont.setPointSize(11);
        modeLabel->setFont(modeFont);
        layout->addWidget(modeLabel);

        // SVPWM hexagon — always visible at top, centered horizontally
        auto* hexagonGroup = QtOwned<QGroupBox>("SVPWM Hexagon (Vβ vs Vα)", this);
        auto* hexagonOuter = QtOwned<QHBoxLayout>();
        hexagonOuter->setContentsMargins(4, 4, 4, 4);

        hexagonWidget = QtOwned<HexagonWidget>(this);
        hexagonWidget->setMinimumSize(hexagonMaxHeight, hexagonMaxHeight);
        hexagonWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        hexagonOuter->addStretch(1);
        hexagonOuter->addWidget(hexagonWidget);
        hexagonOuter->addStretch(1);

        hexagonGroup->setLayout(hexagonOuter);
        hexagonGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        layout->addWidget(hexagonGroup, 0);

        // Tabbed area: phase signals and RLS estimates
        auto* tabs = QtOwned<QTabWidget>(this);

        // Tab 1: Phase Signals
        auto* phaseTab = QtOwned<QWidget>(this);
        auto* phaseSignalsLayout = QtOwned<QVBoxLayout>(phaseTab);

        auto* currentsLabel = QtOwned<QLabel>("Phase Currents (A, B, C)", this);
        QFont sectionFont;
        sectionFont.setBold(true);
        currentsLabel->setFont(sectionFont);
        phaseSignalsLayout->addWidget(currentsLabel);

        currentScope = QtOwned<ScopeWidget>(this);
        currentScope->SetChannelCount(3);
        currentScope->SetChannelConfig(0, { "Ia", QColor(0, 150, 255) });
        currentScope->SetChannelConfig(1, { "Ib", QColor(255, 165, 0) });
        currentScope->SetChannelConfig(2, { "Ic", QColor(0, 200, 80) });

        currentScopeToolbar = QtOwned<ScopeToolbar>(*currentScope, this);
        phaseSignalsLayout->addWidget(currentScopeToolbar);
        phaseSignalsLayout->addWidget(currentScope);

        auto* voltagesLabel = QtOwned<QLabel>("Phase Voltages — inverter midpoint (Va, Vb, Vc)", this);
        voltagesLabel->setFont(sectionFont);
        phaseSignalsLayout->addWidget(voltagesLabel);

        voltageScope = QtOwned<ScopeWidget>(this);
        voltageScope->SetChannelCount(3);
        voltageScope->SetChannelConfig(0, { "Va", QColor(0, 150, 255) });
        voltageScope->SetChannelConfig(1, { "Vb", QColor(255, 165, 0) });
        voltageScope->SetChannelConfig(2, { "Vc", QColor(0, 200, 80) });

        voltageScopeToolbar = QtOwned<ScopeToolbar>(*voltageScope, this);
        phaseSignalsLayout->addWidget(voltageScopeToolbar);
        phaseSignalsLayout->addWidget(voltageScope);

        tabs->addTab(phaseTab, "Phase Signals");

        // Tab 2: Online RLS Estimates
        auto* rlsTab = QtOwned<QWidget>(this);
        auto* rlsLayout = QtOwned<QVBoxLayout>(rlsTab);

        electricalRlsScope = QtOwned<ScopeWidget>(this);
        electricalRlsScope->SetChannelCount(2);
        electricalRlsScope->SetChannelConfig(0, { "R\xCC\x82 [\xCE\xA9]", QColor(220, 50, 50) });
        electricalRlsScope->SetChannelConfig(1, { "L\xCC\x82 [mH]", QColor(0, 220, 220) });
        rlsLayout->addWidget(electricalRlsScope);

        mechanicalRlsScope = QtOwned<ScopeWidget>(this);
        mechanicalRlsScope->SetChannelCount(2);
        mechanicalRlsScope->SetChannelConfig(0, { "B\xCC\x82 [\xC2\xB5N\xC2\xB7m\xC2\xB7s/rad]", QColor(220, 200, 0) });
        mechanicalRlsScope->SetChannelConfig(1, { "J\xCC\x82 [\xC2\xB5kg\xC2\xB7m\xC2\xB2]", QColor(0, 200, 80) });
        rlsLayout->addWidget(mechanicalRlsScope);

        tabs->addTab(rlsTab, "RLS Estimates");

        layout->addWidget(tabs, 1);
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

    void ScopesPanel::SetMode(const QString& label)
    {
        modeLabel->setText(label);
    }

    void ScopesPanel::Clear()
    {
        currentScope->Clear();
        voltageScope->Clear();
        hexagonWidget->Clear();
        electricalRlsScope->Clear();
        mechanicalRlsScope->Clear();
    }

    void ScopesPanel::AddElectricalRlsSample(float Rhat, float Lhat)
    {
        const std::array<float, 2> a = { Rhat, Lhat * 1000.0f };
        electricalRlsScope->AddSample({ a.data(), 2 });
    }

    void ScopesPanel::AddMechanicalRlsSample(float Bhat, float Jhat)
    {
        const std::array<float, 2> a = { Bhat * 1.0e6f, Jhat * 1.0e6f };
        mechanicalRlsScope->AddSample({ a.data(), 2 });
    }
}
