#pragma once

#include "tools/simulator/view/gui/ScopeWidget.hpp"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QWidget>

namespace simulator
{
    class ScopeToolbar
        : public QWidget
    {
        Q_OBJECT

    public:
        explicit ScopeToolbar(ScopeWidget& scope, QWidget* parent = nullptr);

    private:
        void SetupUi();
        void ConnectSignals();
        void PopulateTimeDivOptions();

        ScopeWidget& scope;

        QComboBox* timeDivCombo;
        QDoubleSpinBox* triggerLevelSpin;
        QComboBox* triggerModeCombo;
        QComboBox* triggerEdgeCombo;
        QComboBox* triggerChannelCombo;
        QPushButton* runStopButton;
        QPushButton* singleButton;
        QPushButton* forceTriggerButton;
    };
}
