#include "source/simulator/view/gui/GuiSimulation.hpp"
#include "foc/implementations/Runner.hpp"
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

    GuiSimulation::GuiSimulation(int& argc, char** argv, ThreePhaseMotorModel& model, foc::Runner& runner, infra::EventDispatcherWithWeakPtr& eventDispatcher,
        const ThreePhaseMotorModel::Parameters& motorParameters, const ParametersPanel::PidParameters& pidParameters)
        : app(InitAndPassArgc(argc), argv)
        , gui(model, runner, motorParameters, pidParameters)
    {
        gui.show();

        QObject::connect(&eventLoopTimer, &QTimer::timeout, [&eventDispatcher]()
            {
                eventDispatcher.ExecuteAllActions();
            });
        eventLoopTimer.start(0);
    }

    Gui& GuiSimulation::GetGui()
    {
        return gui;
    }

    int GuiSimulation::Run()
    {
        return app.exec();
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
