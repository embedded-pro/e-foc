#pragma once

#include "core/foc/interfaces/Driver.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "tools/simulator/model/Model.hpp"
#include "tools/simulator/view/gui/Gui.hpp"
#include "tools/simulator/view/gui/ParametersPanel.hpp"
#include <QTimer>

namespace simulator
{
    class GuiSimulation
    {
    public:
        GuiSimulation(ThreePhaseMotorModel& model, foc::Controllable& controller, infra::EventDispatcherWithWeakPtr& eventDispatcher,
            const ThreePhaseMotorModel::Parameters& motorParameters, const ParametersPanel::PidParameters& pidParameters,
            const ControlPanel::SetpointConfig& setpointConfig, foc::Volts powerSupplyVoltage);

        Gui& GetGui();
        int Run() const;

        static void Init();

    private:
        static void MessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg);

        Gui gui;
        QTimer eventLoopTimer;
    };
}
