#include "tools/simulator/view/gui/ScopesPanel.hpp"
#include "tools/simulator/view/gui/QtOwned.hpp"
#include "ui/theme/Theme.hpp"
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>

namespace simulator
{
    namespace
    {
        constexpr int hexagonMaxHeight = 460;
        constexpr int refreshIntervalMs = 33;
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

        hexagonWidget = QtOwned<ui::backend::qt::QtPaintedWidget>(hexagonCore, this);
        hexagonWidget->SetBackgroundRole(ui::theme::ColorRole::ScopeBackground);
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

        currentScope = QtOwned<ui::backend::qt::QtPaintedWidget>(currentScopeCore, this);
        currentScope->SetBackgroundRole(ui::theme::ColorRole::ScopeBackground);
        currentScopeCore.SetChannelCount(3);
        currentScopeCore.SetChannelConfig(0, { "Ia", ui::Color{ 0, 150, 255 } });
        currentScopeCore.SetChannelConfig(1, { "Ib", ui::Color{ 255, 165, 0 } });
        currentScopeCore.SetChannelConfig(2, { "Ic", ui::Color{ 0, 200, 80 } });

        currentScopeToolbar = QtOwned<ScopeToolbar>(currentScopeCore, this);
        phaseSignalsLayout->addWidget(currentScopeToolbar);
        phaseSignalsLayout->addWidget(currentScope);

        auto* voltagesLabel = QtOwned<QLabel>("Phase Voltages — inverter midpoint (Va, Vb, Vc)", this);
        voltagesLabel->setFont(sectionFont);
        phaseSignalsLayout->addWidget(voltagesLabel);

        voltageScope = QtOwned<ui::backend::qt::QtPaintedWidget>(voltageScopeCore, this);
        voltageScope->SetBackgroundRole(ui::theme::ColorRole::ScopeBackground);
        voltageScopeCore.SetChannelCount(3);
        voltageScopeCore.SetChannelConfig(0, { "Va", ui::Color{ 0, 150, 255 } });
        voltageScopeCore.SetChannelConfig(1, { "Vb", ui::Color{ 255, 165, 0 } });
        voltageScopeCore.SetChannelConfig(2, { "Vc", ui::Color{ 0, 200, 80 } });

        voltageScopeToolbar = QtOwned<ScopeToolbar>(voltageScopeCore, this);
        phaseSignalsLayout->addWidget(voltageScopeToolbar);
        phaseSignalsLayout->addWidget(voltageScope);

        tabs->addTab(phaseTab, "Phase Signals");

        // Tab 2: Online RLS Estimates
        auto* rlsTab = QtOwned<QWidget>(this);
        auto* rlsLayout = QtOwned<QVBoxLayout>(rlsTab);

        electricalRlsScope = QtOwned<ui::backend::qt::QtPaintedWidget>(electricalRlsScopeCore, this);
        electricalRlsScope->SetBackgroundRole(ui::theme::ColorRole::ScopeBackground);
        electricalRlsScopeCore.SetChannelCount(2);
        electricalRlsScopeCore.SetChannelConfig(0, { "R\xCC\x82 [\xCE\xA9]", ui::Color{ 220, 50, 50 } });
        electricalRlsScopeCore.SetChannelConfig(1, { "L\xCC\x82 [mH]", ui::Color{ 0, 220, 220 } });
        rlsLayout->addWidget(electricalRlsScope);

        mechanicalRlsScope = QtOwned<ui::backend::qt::QtPaintedWidget>(mechanicalRlsScopeCore, this);
        mechanicalRlsScope->SetBackgroundRole(ui::theme::ColorRole::ScopeBackground);
        mechanicalRlsScopeCore.SetChannelCount(2);
        mechanicalRlsScopeCore.SetChannelConfig(0, { "B\xCC\x82 [\xC2\xB5N\xC2\xB7m\xC2\xB7s/rad]", ui::Color{ 220, 200, 0 } });
        mechanicalRlsScopeCore.SetChannelConfig(1, { "J\xCC\x82 [\xC2\xB5kg\xC2\xB7m\xC2\xB2]", ui::Color{ 0, 200, 80 } });
        rlsLayout->addWidget(mechanicalRlsScope);

        tabs->addTab(rlsTab, "RLS Estimates");

        layout->addWidget(tabs, 1);

        // Neither HexagonCore::SetSample nor ScopeCore::AddSample requests a repaint, so that a
        // 20 kHz sample path never drives the display; the host owns the cadence for all five.
        for (auto* painted : { hexagonWidget, currentScope, voltageScope, electricalRlsScope, mechanicalRlsScope })
            connect(&refreshTimer, &QTimer::timeout, painted, QOverload<>::of(&QWidget::update));

        refreshTimer.start(refreshIntervalMs);
    }

    void ScopesPanel::AddCurrentSample(std::span<const float> sample)
    {
        currentScopeCore.AddSample(sample);
    }

    void ScopesPanel::AddVoltageSample(std::span<const float> sample)
    {
        voltageScopeCore.AddSample(sample);
    }

    void ScopesPanel::SetHexagonSample(float va, float vb, float vc, float vAlpha, float vBeta)
    {
        hexagonCore.SetSample(va, vb, vc, vAlpha, vBeta);
    }

    void ScopesPanel::SetDcLink(foc::Volts vdc)
    {
        hexagonCore.SetDcLinkVolts(vdc.Value());
    }

    void ScopesPanel::SetMode(const QString& label)
    {
        modeLabel->setText(label);
    }

    void ScopesPanel::Clear()
    {
        currentScopeCore.Clear();
        voltageScopeCore.Clear();
        hexagonCore.Clear();
        electricalRlsScopeCore.Clear();
        mechanicalRlsScopeCore.Clear();
    }

    void ScopesPanel::AddElectricalRlsSample(float Rhat, float Lhat)
    {
        const std::array<float, 2> a = { Rhat, Lhat * 1000.0f };
        electricalRlsScopeCore.AddSample({ a.data(), 2 });
    }

    void ScopesPanel::AddMechanicalRlsSample(float Bhat, float Jhat)
    {
        const std::array<float, 2> a = { Bhat * 1.0e6f, Jhat * 1.0e6f };
        mechanicalRlsScopeCore.AddSample({ a.data(), 2 });
    }
}
