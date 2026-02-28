#include "args.hxx"
#include "foc/implementations/Runner.hpp"
#include "foc/interfaces/Units.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "simulator/model/Jk42bls01X038ed.hpp"
#include "simulator/model/Model.hpp"
#include "simulator/view/Plot.hpp"
#include "source/foc/instantiations/FocImpl.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    constexpr int microsecondsPerSecond = 1000000;

    void ValidateInputs(float powerSupplyVoltage, int timeStepUs, int simulationTimeMs)
    {
        if (powerSupplyVoltage <= 0.0f)
            throw std::invalid_argument("Power supply voltage must be positive (got " + std::to_string(powerSupplyVoltage) + " V)");
        if (timeStepUs <= 0)
            throw std::invalid_argument("Time step must be positive (got " + std::to_string(timeStepUs) + " us)");
        if (simulationTimeMs <= 0)
            throw std::invalid_argument("Simulation time must be positive (got " + std::to_string(simulationTimeMs) + " ms)");
    }
}

int main(int argc, char* argv[])
{
    std::string toolname = argv[0];
    args::ArgumentParser parser(toolname + " is a tool to simulate FOC torque control.");
    args::Group positionals(parser, "Positional arguments:");
    args::Positional currentSetPointArgument(positionals, "currentSetPoint", "current set point for the simulation (in Amperes) [default = 0.1 A]", 0.1f, args::Options::Single);
    args::Positional kpTorqueArgument(positionals, "kpTorque", "proportional gain for the torque controller [default = 15.0]", 15.0f, args::Options::Single);
    args::Positional kiTorqueArgument(positionals, "kiTorque", "integral gain for the torque controller [default = 2000.0]", 2000.0f, args::Options::Single);
    args::Positional kdTorqueArgument(positionals, "kdTorque", "derivative gain for the torque controller [default = 0.0]", 0.0f, args::Options::Single);
    args::Positional powerSupplyVoltageArgument(positionals, "powerSupplyVoltage", "power supply voltage for the simulation (in Volts) [default = 24 Volts]", 24.0f, args::Options::Single);
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

        ValidateInputs(powerSupplyVoltage, timeStepUs, simulationTimeMs);

        const auto timeStep = std::chrono::microseconds{ timeStepUs };
        const auto simulationTime = std::chrono::milliseconds{ simulationTimeMs };
        const auto baseFrequency = hal::Hertz{ static_cast<uint32_t>(microsecondsPerSecond / timeStepUs) };
        const auto steps = static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::duration<float>>(simulationTime).count() / std::chrono::duration_cast<std::chrono::duration<float>>(timeStep).count());

        bool simulationFinished = false;

        simulator::ThreePhaseMotorModel model{ simulator::JK42BLS01_X038ED::parameters, foc::Volts{ powerSupplyVoltage }, baseFrequency, steps };
        simulator::Plot plotter{ model, "FOC Torque Control", "foc_torque_results", std::string(PROJECT_ROOT_DIR) + "/simulator/torque_control", timeStep, simulationTime };
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
