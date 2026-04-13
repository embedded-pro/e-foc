#include "args.hxx"
#include "foc/instantiations/FocController.hpp"
#include "foc/interfaces/Units.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "tools/simulator/headless/HeadlessSimulation.hpp"
#include "tools/simulator/model/Jk42bls01X038ed.hpp"
#include "tools/simulator/model/Model.hpp"
#include "tools/simulator/view/gui/GuiSimulation.hpp"
#include "tools/simulator/view/gui/ParametersPanel.hpp"
#include <chrono>
#include <format>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace
{
    constexpr int microsecondsPerSecond = 1000000;

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
    args::Flag noGuiFlag(parser, "no-gui", "Run without GUI (headless mode).", { "no-gui" });
    args::HelpFlag help(parser, "help", "display this help menu.", { 'h', "help" });

    try
    {
        parser.ParseCLI(argc, argv);
    }
    catch (const args::Help&)
    {
        std::cout << parser;
        return 0;
    }
    catch (const args::ParseError& ex)
    {
        std::cerr << ex.what() << std::endl;
        return 1;
    }

    const bool enableGui = !noGuiFlag;

    try
    {
        infra::EventDispatcherWithWeakPtr::WithSize<50> eventDispatcher;

        const auto timeStepUs = args::get(timeStepArgument);
        const auto simulationTimeMs = args::get(simulationTimeArgument);
        const auto powerSupplyVoltage = args::get(powerSupplyVoltageArgument);
        const auto loadTorque = args::get(loadTorqueArgument);

        ValidateInputs(powerSupplyVoltage, loadTorque, timeStepUs, simulationTimeMs);

        const auto timeStep = std::chrono::microseconds{ timeStepUs };
        const auto simulationTime = std::chrono::milliseconds{ simulationTimeMs };
        const auto baseFrequency = hal::Hertz{ static_cast<uint32_t>(microsecondsPerSecond / timeStepUs) };
        const auto steps = static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::duration<float>>(simulationTime).count() / std::chrono::duration_cast<std::chrono::duration<float>>(timeStep).count());

        simulator::ThreePhaseMotorModel model{ simulator::JK42BLS01_X038ED::parameters, foc::Volts{ powerSupplyVoltage }, baseFrequency, enableGui ? std::optional<std::size_t>{} : std::optional<std::size_t>{ steps } };
        if (loadTorque > 0.0f)
            model.SetLoad(foc::NewtonMeter{ loadTorque });

        foc::FocTorqueController controller{ model, model };

        controller.SetCurrentTunings(foc::Volts{ powerSupplyVoltage },
            foc::IdAndIqTunings{
                { args::get(kpTorqueArgument), args::get(kiTorqueArgument), args::get(kdTorqueArgument) },
                { args::get(kpTorqueArgument), args::get(kiTorqueArgument), args::get(kdTorqueArgument) } });
        controller.SetPolePairs(simulator::JK42BLS01_X038ED::parameters.p);
        controller.SetPoint(foc::IdAndIqPoint{ foc::Ampere{ 0.0f }, foc::Ampere{ args::get(currentSetPointArgument) } });

        if (enableGui)
        {
            const simulator::ParametersPanel::PidParameters pidParameters{
                .current = { args::get(kpTorqueArgument), args::get(kiTorqueArgument), args::get(kdTorqueArgument) }
            };

            const simulator::ControlPanel::SetpointConfig torqueSetpointConfig{
                .label = "Current Setpoint:",
                .unit = "A",
                .min = -15,
                .max = 15,
                .tickInterval = 1,
                .defaultValue = 0
            };

            simulator::GuiSimulation simulation{ argc, argv, model, controller, eventDispatcher, simulator::JK42BLS01_X038ED::parameters, pidParameters, torqueSetpointConfig };
            return simulation.Run();
        }

        simulator::HeadlessSimulation simulation{ model, controller, eventDispatcher,
            simulator::HeadlessSimulationConfig{
                .title = "FOC Torque Control",
                .filename = "foc_torque_results",
                .outputDirectory = std::format("{}/output/simulator/torque_control", PROJECT_ROOT_DIR),
                .timeStep = timeStep,
                .simulationTime = simulationTime,
            } };
        simulation.Run();
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
