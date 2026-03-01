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
#include <format>
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

    void ValidateInputs(float powerSupplyVoltage, float maxCurrent, float loadTorque, int timeStepUs, int simulationTimeMs)
    {
        if (powerSupplyVoltage <= 0.0f)
            throw std::invalid_argument(std::format("Power supply voltage must be positive (got {} V)", powerSupplyVoltage));
        if (maxCurrent <= 0.0f)
            throw std::invalid_argument(std::format("Maximum current must be positive (got {} A)", maxCurrent));
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
    QApplication app(argc, argv);

    args::ArgumentParser parser(std::format("{} is a tool to simulate FOC speed control.", argv[0]));
    args::Group positionals(parser, "Positional arguments:");
    args::Positional speedSetPointArgument(positionals, "speedSetPoint", "speed set point for the simulation (in RPM) [default = 100.0 RPM]", 100.0f, args::Options::Single);
    args::Positional kpTorqueArgument(positionals, "kpTorque", "proportional gain for the torque controller [default = 15.0]", 15.0f, args::Options::Single);
    args::Positional kiTorqueArgument(positionals, "kiTorque", "integral gain for the torque controller [default = 2000.0]", 2000.0f, args::Options::Single);
    args::Positional kdTorqueArgument(positionals, "kdTorque", "derivative gain for the torque controller [default = 0.0]", 0.0f, args::Options::Single);
    args::Positional kpSpeedArgument(positionals, "kpSpeed", "proportional gain for the speed controller [default = 0.1]", 0.1f, args::Options::Single);
    args::Positional kiSpeedArgument(positionals, "kiSpeed", "integral gain for the speed controller [default = 5.0]", 5.0f, args::Options::Single);
    args::Positional kdSpeedArgument(positionals, "kdSpeed", "derivative gain for the speed controller [default = 0.0]", 0.0f, args::Options::Single);
    args::Positional powerSupplyVoltageArgument(positionals, "powerSupplyVoltage", "power supply voltage for the simulation (in Volts) [default = 24 Volts]", 24.0f, args::Options::Single);
    args::Positional maxCurrentArgument(positionals, "maxCurrent", "maximum current for the simulation (in Amperes) [default = 15.0 A]", 15.0f, args::Options::Single);
    args::Positional loadTorqueArgument(positionals, "loadTorque", "load torque for the simulation (in N·m) [default = 0.02 Nm]", 0.02f, args::Options::Single);
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
        const auto loadTorque = args::get(loadTorqueArgument);

        ValidateInputs(powerSupplyVoltage, maxCurrent, loadTorque, timeStepUs, simulationTimeMs);

        const auto timeStep = std::chrono::microseconds{ timeStepUs };
        const auto simulationTime = std::chrono::milliseconds{ simulationTimeMs };
        const auto baseFrequency = hal::Hertz{ static_cast<uint32_t>(microsecondsPerSecond / timeStepUs) };
        const auto steps = static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::duration<float>>(simulationTime).count() / std::chrono::duration_cast<std::chrono::duration<float>>(timeStep).count());

        bool simulationFinished = false;

        simulator::ThreePhaseMotorModel model{ simulator::JK42BLS01_X038ED::parameters, foc::Volts{ powerSupplyVoltage }, baseFrequency, steps };
        if (loadTorque > 0.0f)
            model.SetLoad(foc::NewtonMeter{ loadTorque });
        simulator::Plot plotter{ model, "FOC Speed Control", "foc_speed_results", std::format("{}/output/simulator/speed_control", PROJECT_ROOT_DIR), timeStep, simulationTime };

        simulator::Gui gui;
        gui.show();

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
