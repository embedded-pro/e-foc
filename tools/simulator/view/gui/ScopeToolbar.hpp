#pragma once

#include "ui/scope/ScopeCore.hpp"
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
        explicit ScopeToolbar(ui::scope::ScopeCore& scope, QWidget* parent = nullptr);

    private:
        void SetupUi();
        void ConnectSignals();
        void PopulateTimeDivOptions();

        ui::scope::ScopeCore& scope;

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
