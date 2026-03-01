#include "source/simulator/view/gui/Gui.hpp"
#include "foc/implementations/Runner.hpp"
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

    Gui::Gui(ThreePhaseMotorModel& model, foc::Runner& runner, const ThreePhaseMotorModel::Parameters& motorParameters, const ParametersPanel::PidParameters& pidParameters, QWidget* parent)
        : QMainWindow(parent)
        , ThreePhaseMotorModelObserver(model)
        , runner(runner)
    {
        setWindowTitle("e-foc Simulator");
        setFixedSize(windowWidth, windowHeight);

        auto* centralWidget = QtOwned<QWidget>(this);
        setCentralWidget(centralWidget);

        controlPanel = QtOwned<ControlPanel>(this);
        parametersPanel = QtOwned<ParametersPanel>(motorParameters, pidParameters, this);
        scopesPanel = QtOwned<ScopesPanel>(this);

        auto* mainLayout = QtOwned<QHBoxLayout>(centralWidget);
        mainLayout->addWidget(controlPanel, leftPanelStretch);
        mainLayout->addWidget(parametersPanel, middlePanelStretch);
        mainLayout->addWidget(scopesPanel, rightPanelStretch);

        connect(controlPanel, &ControlPanel::startClicked, this, [this]()
            {
                this->runner.Enable();
            });

        connect(controlPanel, &ControlPanel::stopClicked, this, [this]()
            {
                this->runner.Disable();
            });

        connect(controlPanel, &ControlPanel::speedChanged, this, &Gui::speedChanged);
    }

    void Gui::Started()
    {
        controlPanel->SetStatus("Running");
        hasPreviousTheta = false;
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
}
