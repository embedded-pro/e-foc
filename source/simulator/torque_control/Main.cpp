#include "args.hxx"
#include "foc/implementations/Runner.hpp"
#include "foc/interfaces/Units.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "simulator/model/Jk42bls01X038ed.hpp"
#include "simulator/model/Model.hpp"
#include "simulator/view/gui/Gui.hpp"
#include "simulator/view/plot/Plot.hpp"
#include "source/foc/instantiations/FocImpl.hpp"
#include <QApplication>
#include <chrono>
#include <cstdlib>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    constexpr int microsecondsPerSecond = 1000000;

    void GuiMessageHandler(QtMsgType type, const QMessageLogContext& /*context*/, const QString& msg)
    {
        if (type == QtFatalMsg)
        {
            std::cerr << "GUI initialization failed: " << msg.toStdString() << "\n\n"
                      << "To use the GUI, start an X server (e.g., VcXsrv on Windows) and set:\n"
                      << "  export DISPLAY=host.docker.internal:0.0\n\n"
                      << "To run without GUI, set:\n"
                      << "  export QT_QPA_PLATFORM=offscreen\n";
            std::exit(1); // NOLINT
        }

        std::cerr << msg.toStdString() << std::endl;
    }

    void ValidateInputs(float powerSupplyVoltage, float loadTorque, int timeStepUs, int simulationTimeMs)
    {
        if (powerSupplyVoltage <= 0.0f)
            throw std::invalid_argument(std::format("Power supply voltage must be positive (got {} V)", powerSupplyVoltage));
        if (loadTorque < 0.0f)
            throw std::invalid_argument(std::format("Load torque must be non-negative (got {} Nm)", loadTorque));
        if (timeStepUs <= 0)
            throw std::invalid_argument(std::format("Time step must be positive (got {} us)", timeStepUs));
        if (simulationTimeMs <= 0)
            throw std::invalid_argument(std::format("Simulation time must be positive (got {} ms)", simulationTimeMs));
    }
}

int main(int argc, char* argv[])
{
    if (qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
        qputenv("DISPLAY", "host.docker.internal:0.0");

    qInstallMessageHandler(GuiMessageHandler);
    QApplication app(argc, argv);

    args::ArgumentParser parser(std::format("{} is a tool to simulate FOC torque control.", argv[0]));
    args::Group positionals(parser, "Positional arguments:");
    args::Positional currentSetPointArgument(positionals, "currentSetPoint", "current set point for the simulation (in Amperes) [default = 0.1 A]", 0.1f, args::Options::Single);
    args::Positional kpTorqueArgument(positionals, "kpTorque", "proportional gain for the torque controller [default = 15.0]", 15.0f, args::Options::Single);
    args::Positional kiTorqueArgument(positionals, "kiTorque", "integral gain for the torque controller [default = 2000.0]", 2000.0f, args::Options::Single);
    args::Positional kdTorqueArgument(positionals, "kdTorque", "derivative gain for the torque controller [default = 0.0]", 0.0f, args::Options::Single);
    args::Positional powerSupplyVoltageArgument(positionals, "powerSupplyVoltage", "power supply voltage for the simulation (in Volts) [default = 24 Volts]", 24.0f, args::Options::Single);
    args::Positional loadTorqueArgument(positionals, "loadTorque", "load torque for the simulation (in N\xC2\xB7m) [default = 0.02 Nm]", 0.02f, args::Options::Single);
    args::Positional timeStepArgument(positionals, "timeStep", "time step for the simulation (in microseconds) [default = 10 us]", 10, args::Options::Single);
    args::Positional simulationTimeArgument(positionals, "simulationTime", "total simulation time (in milliseconds) [default = 1000 ms]", 1000, args::Options::Single);
    args::HelpFlag help(parser, "help", "display this help menu.", { 'h', "help" });

    try
    {
        infra::EventDispatcherWithWeakPtr::WithSize<50> eventDispatcher;

        parser.ParseCLI(argc, argv);

        const auto timeStepUs = args::get(timeStepArgument);
        const auto simulationTimeMs = args::get(simulationTimeArgument);
        const auto powerSupplyVoltage = args::get(powerSupplyVoltageArgument);
        const auto loadTorque = args::get(loadTorqueArgument);

        ValidateInputs(powerSupplyVoltage, loadTorque, timeStepUs, simulationTimeMs);

        const auto timeStep = std::chrono::microseconds{ timeStepUs };
        const auto simulationTime = std::chrono::milliseconds{ simulationTimeMs };
        const auto baseFrequency = hal::Hertz{ static_cast<uint32_t>(microsecondsPerSecond / timeStepUs) };
        const auto steps = static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::duration<float>>(simulationTime).count() / std::chrono::duration_cast<std::chrono::duration<float>>(timeStep).count());

        bool simulationFinished = false;

        simulator::ThreePhaseMotorModel model{ simulator::JK42BLS01_X038ED::parameters, foc::Volts{ powerSupplyVoltage }, baseFrequency, steps };
        if (loadTorque > 0.0f)
            model.SetLoad(foc::NewtonMeter{ loadTorque });
        simulator::Plot plotter{ model, "FOC Torque Control", "foc_torque_results", std::format("{}/output/simulator/torque_control", PROJECT_ROOT_DIR), timeStep, simulationTime };

        simulator::Gui gui;
        gui.show();

        foc::FocTorqueImpl focTorque;
        foc::Runner focRunner{ model, model, focTorque };

        focTorque.SetCurrentTunings(foc::Volts{ powerSupplyVoltage },
            foc::IdAndIqTunings{
                { args::get(kpTorqueArgument), args::get(kiTorqueArgument), args::get(kdTorqueArgument) },
                { args::get(kpTorqueArgument), args::get(kiTorqueArgument), args::get(kdTorqueArgument) } });
        focTorque.SetPolePairs(simulator::JK42BLS01_X038ED::parameters.p);
        focTorque.SetPoint(foc::IdAndIqPoint{ foc::Ampere{ 0.0f }, foc::Ampere{ args::get(currentSetPointArgument) } });

        simulator::SimulationFinishedObserver finishedObserver{ model, [&simulationFinished]()
            {
                simulationFinished = true;
            } };

        focRunner.Enable();

        eventDispatcher.ExecuteUntil([&simulationFinished]()
            {
                return simulationFinished;
            });
    }
    catch (const args::Help&)
    {
        std::cout << parser;
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
