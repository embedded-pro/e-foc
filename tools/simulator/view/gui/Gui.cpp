#include "tools/simulator/view/gui/Gui.hpp"
#include <QHBoxLayout>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>
#include <array>
#include <memory>

namespace simulator
{
    namespace
    {
        constexpr int windowWidth = 1600;
        constexpr int windowHeight = 900;
        constexpr int leftPanelStretch = 2;
        constexpr int rightPanelStretch = 5;

        template<typename T, typename... Args>
        T* QtOwned(Args&&... args)
        {
            return std::make_unique<T>(std::forward<Args>(args)...).release();
        }
    }

    Gui::Gui(ThreePhaseMotorModel& model, foc::Controllable& controller, infra::EventDispatcherWithWeakPtr& eventDispatcher,
        const ThreePhaseMotorModel::Parameters& motorParameters, const ParametersPanel::PidParameters& pidParameters,
        const ControlPanel::SetpointConfig& setpointConfig, foc::Volts powerSupplyVoltage, QWidget* parent)
        : QMainWindow(parent)
        , ThreePhaseMotorModelObserver(model)
        , model(model)
        , controller(controller)
        , eventDispatcher(eventDispatcher)
    {
        setWindowTitle("e-foc Simulator");
        resize(windowWidth, windowHeight);

        auto* centralWidget = QtOwned<QWidget>(this);
        setCentralWidget(centralWidget);

        controlPanel = QtOwned<ControlPanel>(setpointConfig, this);
        parametersPanel = QtOwned<ParametersPanel>(motorParameters, pidParameters, this);
        scopesPanel = QtOwned<ScopesPanel>(this);
        scopesPanel->SetDcLink(powerSupplyVoltage);

        // Left column: control + parameters in a scrollable container
        auto* leftContainer = QtOwned<QWidget>(this);
        auto* leftLayout = QtOwned<QVBoxLayout>(leftContainer);
        leftLayout->addWidget(controlPanel);
        leftLayout->addWidget(parametersPanel);
        leftLayout->addStretch();

        auto* leftScroll = QtOwned<QScrollArea>(this);
        leftScroll->setWidget(leftContainer);
        leftScroll->setWidgetResizable(true);
        leftScroll->setFrameShape(QFrame::NoFrame);

        // Right column: scopes in a scrollable container (4 plots)
        auto* rightScroll = QtOwned<QScrollArea>(this);
        rightScroll->setWidget(scopesPanel);
        rightScroll->setWidgetResizable(true);
        rightScroll->setFrameShape(QFrame::NoFrame);

        auto* mainLayout = QtOwned<QHBoxLayout>(centralWidget);
        mainLayout->addWidget(leftScroll, leftPanelStretch);
        mainLayout->addWidget(rightScroll, rightPanelStretch);

        connect(controlPanel, &ControlPanel::startClicked, this, [this]()
            {
                while (!this->eventDispatcher.IsIdle())
                    this->eventDispatcher.ExecuteFirstAction();

                this->controller.Start();
            });

        connect(controlPanel, &ControlPanel::stopClicked, this, [this]()
            {
                this->controller.Stop();
            });

        connect(controlPanel, &ControlPanel::setpointChanged, this, &Gui::setpointChanged);

        connect(controlPanel, &ControlPanel::loadChanged, this, [this](double torqueNm)
            {
                this->model.SetLoad(foc::NewtonMeter{ static_cast<float>(torqueNm) });
            });

        connect(controlPanel, &ControlPanel::alignClicked, this, &Gui::alignRequested);
        connect(controlPanel, &ControlPanel::identifyElectricalClicked, this, &Gui::identifyElectricalRequested);
        connect(controlPanel, &ControlPanel::identifyMechanicalClicked, this, &Gui::identifyMechanicalRequested);
    }

    void Gui::Started()
    {
        controlPanel->SetStatus("Running");
        scopesPanel->Clear();
    }

    void Gui::PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currentPhases, foc::Radians /*theta*/)
    {
        const std::array<float, 3> currents = { currentPhases.a.Value(), currentPhases.b.Value(), currentPhases.c.Value() };
        scopesPanel->AddCurrentSample(currents);
    }

    void Gui::StatorVoltages(foc::ThreePhase phaseVoltages, foc::TwoPhase alphaBeta)
    {
        const std::array<float, 3> voltages = { phaseVoltages.a, phaseVoltages.b, phaseVoltages.c };
        scopesPanel->AddVoltageSample(voltages);
        scopesPanel->SetHexagonSample(phaseVoltages.a, phaseVoltages.b, phaseVoltages.c, alphaBeta.alpha, alphaBeta.beta);
    }

    void Gui::Finished()
    {
        controlPanel->SetStatus("Finished");
        controlPanel->SetEditable(true);
    }

    void Gui::UpdatePidParameters(const ParametersPanel::PidParameters& pidParameters)
    {
        parametersPanel->UpdatePidParameters(pidParameters);
    }

    void Gui::SetIdentifiedElectrical(foc::Ohm resistance, foc::MilliHenry inductance)
    {
        parametersPanel->UpdateResistance(resistance);
        parametersPanel->UpdateInductance(inductance);
    }

    void Gui::SetIdentifiedPolePairs(std::size_t polePairs)
    {
        parametersPanel->UpdatePolePairs(polePairs);
    }

    void Gui::SetIdentifiedMechanical(foc::NewtonMeterSecondPerRadian friction, foc::NewtonMeterSecondSquared inertia)
    {
        parametersPanel->UpdateFriction(friction);
        parametersPanel->UpdateInertia(inertia);
    }

    void Gui::SetAlignmentOffset(foc::Radians offset)
    {
        parametersPanel->UpdateAlignmentOffset(offset);
    }

    void Gui::CalibrationFinished()
    {
        controlPanel->CalibrationFinished();
    }

    void Gui::DisableMechanicalIdent()
    {
        controlPanel->DisableMechanicalIdent();
    }
}
