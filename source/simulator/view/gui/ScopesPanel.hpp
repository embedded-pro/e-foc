#pragma once

#include "source/simulator/view/gui/ScopeToolbar.hpp"
#include "source/simulator/view/gui/ScopeWidget.hpp"
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
        void AddPositionSpeedSample(std::span<const float> sample);
        void Clear();

    private:
        ScopeWidget* currentScope;
        ScopeToolbar* currentScopeToolbar;
        ScopeWidget* positionSpeedScope;
        ScopeToolbar* positionSpeedScopeToolbar;
    };
}
