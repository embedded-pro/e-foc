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
#include <numbers>
#include <stdexcept>
#include <string>

namespace
{
    constexpr float rpmToRadPerSecFactor = std::numbers::pi_v<float> / 30.0f;
    constexpr int microsecondsPerSecond = 1000000;

    foc::RadiansPerSecond ToRadiansPerSecond(float rpm)
    {
        return foc::RadiansPerSecond{ rpm * rpmToRadPerSecFactor };
    }

    void ValidateInputs(float powerSupplyVoltage, float maxCurrent, int timeStepUs, int simulationTimeMs)
    {
        if (powerSupplyVoltage <= 0.0f)
            throw std::invalid_argument("Power supply voltage must be positive (got " + std::to_string(powerSupplyVoltage) + " V)");
        if (maxCurrent <= 0.0f)
            throw std::invalid_argument("Maximum current must be positive (got " + std::to_string(maxCurrent) + " A)");
        if (timeStepUs <= 0)
            throw std::invalid_argument("Time step must be positive (got " + std::to_string(timeStepUs) + " us)");
        if (simulationTimeMs <= 0)
            throw std::invalid_argument("Simulation time must be positive (got " + std::to_string(simulationTimeMs) + " ms)");
    }
}

int main(int argc, char* argv[])
{
    std::string toolname = argv[0];
    args::ArgumentParser parser(toolname + " is a tool to simulate FOC speed control.");
    args::Group positionals(parser, "Positional arguments:");
    args::Positional speedSetPointArgument(positionals, "speedSetPoint", "speed set point for the simulation (in RPM) [default = 100.0 RPM]", 100.0f, args::Options::Single);
    args::Positional kpTorqueArgument(positionals, "kpTorque", "proportional gain for the torque controller [default = 15.0]", 15.0f, args::Options::Single);
    args::Positional kiTorqueArgument(positionals, "kiTorque", "integral gain for the torque controller [default = 2000.0]", 2000.0f, args::Options::Single);
    args::Positional kdTorqueArgument(positionals, "kdTorque", "derivative gain for the torque controller [default = 0.0]", 0.0f, args::Options::Single);
    args::Positional kpSpeedArgument(positionals, "kpSpeed", "proportional gain for the speed controller [default = 2.5f]", 2.5f, args::Options::Single);
    args::Positional kiSpeedArgument(positionals, "kiSpeed", "integral gain for the speed controller [default = 80.0]", 80.0f, args::Options::Single);
    args::Positional kdSpeedArgument(positionals, "kdSpeed", "derivative gain for the speed controller [default = 0.0]", 0.0f, args::Options::Single);
    args::Positional powerSupplyVoltageArgument(positionals, "powerSupplyVoltage", "power supply voltage for the simulation (in Volts) [default = 24 Volts]", 24.0f, args::Options::Single);
    args::Positional maxCurrentArgument(positionals, "maxCurrent", "maximum current for the simulation (in Amperes) [default = 15.0 A]", 15.0f, args::Options::Single);
    args::Positional timeStepArgument(positionals, "timeStep", "time step for the simulation (in microseconds) [default = 10 us]", 10, args::Options::Single);
    args::Positional simulationTimeArgument(positionals, "simulationTime", "total simulation time (in milliseconds) [default = 1000 ms]", 1000, args::Options::Single);
    args::HelpFlag help(parser, "help", "display this help menu.", { 'h', "help" });

    try
    {
        infra::EventDispatcherWithWeakPtr::WithSize<50> eventDispatcher;

        parser.ParseCLI(argc, argv);

        const auto timeStepUs = args::get(timeStepArgument);
        const auto simulationTimeMs = args::get(simulationTimeArgument);
        const auto maxCurrent = args::get(maxCurrentArgument);
        const auto powerSupplyVoltage = args::get(powerSupplyVoltageArgument);

        ValidateInputs(powerSupplyVoltage, maxCurrent, timeStepUs, simulationTimeMs);

        const auto timeStep = std::chrono::microseconds{ timeStepUs };
        const auto simulationTime = std::chrono::milliseconds{ simulationTimeMs };
        const auto baseFrequency = hal::Hertz{ static_cast<uint32_t>(microsecondsPerSecond / timeStepUs) };
        const auto steps = static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::duration<float>>(simulationTime).count() / std::chrono::duration_cast<std::chrono::duration<float>>(timeStep).count());

        bool simulationFinished = false;

        simulator::ThreePhaseMotorModel model{ simulator::JK42BLS01_X038ED::parameters, foc::Volts{ powerSupplyVoltage }, baseFrequency, steps };
        simulator::Plot plotter{ model, "FOC Speed Control", "foc_speed_results", std::string(PROJECT_ROOT_DIR) + "/simulator/speed_control", timeStep, simulationTime };
        foc::FocSpeedImpl focSpeed{ foc::Ampere{ maxCurrent }, timeStep };
        foc::Runner focRunner{ model, model, focSpeed };

        focSpeed.SetSpeedTunings(foc::Volts{ powerSupplyVoltage },
            controllers::PidTunings<float>{ args::get(kpSpeedArgument), args::get(kiSpeedArgument), args::get(kdSpeedArgument) });
        focSpeed.SetCurrentTunings(foc::Volts{ powerSupplyVoltage },
            foc::IdAndIqTunings{
                { args::get(kpTorqueArgument), args::get(kiTorqueArgument), args::get(kdTorqueArgument) },
                { args::get(kpTorqueArgument), args::get(kiTorqueArgument), args::get(kdTorqueArgument) } });
        focSpeed.SetPolePairs(simulator::JK42BLS01_X038ED::parameters.p);
        focSpeed.SetPoint(ToRadiansPerSecond(args::get(speedSetPointArgument)));
        focRunner.Enable();

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
