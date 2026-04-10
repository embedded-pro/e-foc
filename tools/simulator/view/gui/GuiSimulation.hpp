#pragma once

#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "core/foc/interfaces/Driver.hpp"
#include "tools/simulator/model/Model.hpp"
#include "tools/simulator/view/gui/Gui.hpp"
#include "tools/simulator/view/gui/ParametersPanel.hpp"
#include <QApplication>
#include <QTimer>

namespace simulator
{
    class GuiSimulation
    {
    public:
        GuiSimulation(int& argc, char** argv, ThreePhaseMotorModel& model, foc::Controllable& controller, infra::EventDispatcherWithWeakPtr& eventDispatcher,
            const ThreePhaseMotorModel::Parameters& motorParameters, const ParametersPanel::PidParameters& pidParameters,
            const ControlPanel::SetpointConfig& setpointConfig);

        Gui& GetGui();
        int Run() const;

        static void Init();

    private:
        static void MessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg);

        QApplication app;
        Gui gui;
        QTimer eventLoopTimer;
    };
}
