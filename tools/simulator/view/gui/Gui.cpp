#include "tools/simulator/view/gui/Gui.hpp"
#include <QHBoxLayout>
#include <array>
#include <memory>

namespace simulator
{
    namespace
    {
        constexpr int windowWidth = 1400;
        constexpr int windowHeight = 800;
        constexpr int leftPanelStretch = 1;
        constexpr int middlePanelStretch = 2;
        constexpr int rightPanelStretch = 3;

        template<typename T, typename... Args>
        T* QtOwned(Args&&... args)
        {
            return std::make_unique<T>(std::forward<Args>(args)...).release();
        }
    }

    Gui::Gui(ThreePhaseMotorModel& model, foc::Controllable& controller, infra::EventDispatcherWithWeakPtr& eventDispatcher,
        const ThreePhaseMotorModel::Parameters& motorParameters, const ParametersPanel::PidParameters& pidParameters,
        const ControlPanel::SetpointConfig& setpointConfig, QWidget* parent)
        : QMainWindow(parent)
        , ThreePhaseMotorModelObserver(model)
        , model(model)
        , controller(controller)
        , eventDispatcher(eventDispatcher)
    {
        setWindowTitle("e-foc Simulator");
        setFixedSize(windowWidth, windowHeight);

        auto* centralWidget = QtOwned<QWidget>(this);
        setCentralWidget(centralWidget);

        controlPanel = QtOwned<ControlPanel>(setpointConfig, this);
        parametersPanel = QtOwned<ParametersPanel>(motorParameters, pidParameters, this);
        scopesPanel = QtOwned<ScopesPanel>(this);

        auto* mainLayout = QtOwned<QHBoxLayout>(centralWidget);
        mainLayout->addWidget(controlPanel, leftPanelStretch);
        mainLayout->addWidget(parametersPanel, middlePanelStretch);
        mainLayout->addWidget(scopesPanel, rightPanelStretch);

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
    }

    void Gui::Started()
    {
        controlPanel->SetStatus("Running");
        scopesPanel->Clear();
        hasPreviousTheta = false;
        previousTheta = foc::Radians{ 0.0f };
    }

    void Gui::PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currentPhases, foc::Radians theta)
    {
        const std::array<float, 3> currents = { currentPhases.a.Value(), currentPhases.b.Value(), currentPhases.c.Value() };
        scopesPanel->AddCurrentSample(currents);

        float omega = 0.0f;
        if (hasPreviousTheta)
            omega = theta.Value() - previousTheta.Value();

        previousTheta = theta;
        hasPreviousTheta = true;

        const std::array<float, 2> positionSpeed = { theta.Value(), omega };
        scopesPanel->AddPositionSpeedSample(positionSpeed);
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
}
