#pragma once

#include "core/foc/interfaces/Execution.hpp"
#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "tools/simulator/view/gui/ControlPanel.hpp"
#include "tools/simulator/view/gui/ParametersPanel.hpp"
#include "tools/simulator/view/gui/ScopesPanel.hpp"
#include <QMainWindow>
#include <QString>
#include <QTimer>
#include <cstddef>

namespace simulator
{
    class Gui
        : public QMainWindow
        , public foc::ThreePhaseMotorModelObserver
    {
        Q_OBJECT

    public:
        Gui(foc::ThreePhaseMotorModel& motorModel, foc::Controllable& motorController,
            const foc::ThreePhaseMotorModel::Parameters& motorParameters, const ParametersPanel::PidParameters& pidParameters,
            const ControlPanel::SetpointConfig& setpointConfig, foc::Volts powerSupplyVoltage, QWidget* parent = nullptr);

        // foc::ThreePhaseMotorModelObserver
        void Started() override;
        void PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currentPhases, foc::Radians theta, foc::RadiansPerSecond omegaMech) override;
        void StatorVoltages(foc::ThreePhase phaseVoltages, foc::TwoPhase alphaBeta) override;
        void Finished() override;

    signals:
        void setpointChanged(int value);
        void alignRequested();
        void identifyElectricalRequested();
        void identifyMechanicalRequested();

    public:
        void OnElectricalRlsUpdate(float Rhat, float Lhat)
        {
            scopesPanel->AddElectricalRlsSample(Rhat, Lhat);
        }

        void OnMechanicalRlsUpdate(float Bhat, float Jhat)
        {
            scopesPanel->AddMechanicalRlsSample(Bhat, Jhat);
        }

        void UpdatePidParameters(const ParametersPanel::PidParameters& pidParameters);
        void SetStatus(const QString& status);
        void SetState(const state_machine::State& state);
        static QString LabelFor(const state_machine::State& state);
        void SetIdentifiedElectrical(foc::Ohm resistance, foc::MilliHenry inductance);
        void SetIdentifiedPolePairs(std::size_t polePairs);
        void SetIdentifiedMechanical(foc::NewtonMeterSecondPerRadian friction, foc::NewtonMeterSecondSquared inertia);
        void SetAlignmentOffset(foc::Radians offset);
        void CalibrationFinished();
        void DisableMechanicalIdent();

    private:
        foc::ThreePhaseMotorModel& model;
        foc::Controllable& controller;

        ControlPanel* controlPanel;
        ParametersPanel* parametersPanel;
        ScopesPanel* scopesPanel;
        QTimer* thermalTimer{ nullptr };
    };
}
