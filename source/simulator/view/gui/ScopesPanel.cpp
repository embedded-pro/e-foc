#include "source/simulator/view/gui/ScopesPanel.hpp"
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
        layout->addWidget(currentGroup);

        // Position & Speed scope
        auto* positionSpeedGroup = QtOwned<QGroupBox>("Position && Speed", this);
        auto* positionSpeedLayout = QtOwned<QVBoxLayout>();

        positionSpeedScope = QtOwned<ScopeWidget>(this);
        positionSpeedScope->SetChannelCount(2);
        positionSpeedScope->SetChannelConfig(0, { "θ (rad)", QColor(0, 150, 255) });
        positionSpeedScope->SetChannelConfig(1, { "ω (rad/s)", QColor(255, 100, 100) });

        positionSpeedScopeToolbar = QtOwned<ScopeToolbar>(*positionSpeedScope, this);

        positionSpeedLayout->addWidget(positionSpeedScopeToolbar);
        positionSpeedLayout->addWidget(positionSpeedScope);

        positionSpeedGroup->setLayout(positionSpeedLayout);
        layout->addWidget(positionSpeedGroup);
    }

    void ScopesPanel::AddCurrentSample(std::span<const float> sample)
    {
        currentScope->AddSample(sample);
    }

    void ScopesPanel::AddPositionSpeedSample(std::span<const float> sample)
    {
        positionSpeedScope->AddSample(sample);
    }

    void ScopesPanel::Clear()
    {
        currentScope->Clear();
        positionSpeedScope->Clear();
    }
}
