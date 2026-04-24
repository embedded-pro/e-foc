#include "tools/simulator/view/gui/GuiSimulation.hpp"
#include <cstdlib>
#include <iostream>

namespace simulator
{
    namespace
    {
        int& InitAndPassArgc(int& argc)
        {
            GuiSimulation::Init();
            return argc;
        }
    }

    GuiSimulation::GuiSimulation(int& argc, char** argv, ThreePhaseMotorModel& model, foc::Controllable& controller, infra::EventDispatcherWithWeakPtr& eventDispatcher,
        const ThreePhaseMotorModel::Parameters& motorParameters, const ParametersPanel::PidParameters& pidParameters,
        const ControlPanel::SetpointConfig& setpointConfig, foc::Volts powerSupplyVoltage)
        : app(InitAndPassArgc(argc), argv)
        , gui(model, controller, eventDispatcher, motorParameters, pidParameters, setpointConfig, powerSupplyVoltage)
    {
        gui.show();

        QObject::connect(&eventLoopTimer, &QTimer::timeout, [&eventDispatcher]()
            {
                constexpr int batchSize = 1000;

                for (int i = 0; i < batchSize && !eventDispatcher.IsIdle(); ++i)
                    eventDispatcher.ExecuteFirstAction();
            });
        eventLoopTimer.start(0);
    }

    Gui& GuiSimulation::GetGui()
    {
        return gui;
    }

    int GuiSimulation::Run() const
    {
        return QApplication::exec();
    }

    void GuiSimulation::Init()
    {
        if (qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
            qputenv("DISPLAY", "host.docker.internal:0.0");
        qInstallMessageHandler(MessageHandler);
    }

    void GuiSimulation::MessageHandler(QtMsgType type, const QMessageLogContext& /*context*/, const QString& msg)
    {
        if (type == QtFatalMsg)
        {
            std::cerr << "GUI initialization failed: " << msg.toStdString() << "\n\n"
                      << "To use the GUI, start an X server (e.g., VcXsrv on Windows) and set:\n"
                      << "  export DISPLAY=host.docker.internal:0.0\n\n"
                      << "To run without GUI, pass --no-gui or set:\n"
                      << "  export QT_QPA_PLATFORM=offscreen\n";
            std::exit(1); // NOLINT
        }

        std::cerr << msg.toStdString() << std::endl;
    }
}
