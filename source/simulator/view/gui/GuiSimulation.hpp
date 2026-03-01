#pragma once

#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "source/simulator/model/Model.hpp"
#include "source/simulator/view/gui/Gui.hpp"
#include "source/simulator/view/gui/ParametersPanel.hpp"
#include <QApplication>
#include <QTimer>

namespace simulator
{
    class GuiSimulation
    {
    public:
        GuiSimulation(int& argc, char** argv, ThreePhaseMotorModel& model, foc::Runner& runner, infra::EventDispatcherWithWeakPtr& eventDispatcher,
            const ThreePhaseMotorModel::Parameters& motorParameters, const ParametersPanel::PidParameters& pidParameters);

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
