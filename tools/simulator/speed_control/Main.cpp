#include "args.hxx"
#include "core/services/alignment/MotorAlignmentImpl.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentificationImpl.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentificationImpl.hpp"
#include "foc/implementations/LowPriorityInterruptImpl.hpp"
#include "foc/instantiations/FocController.hpp"
#include "foc/interfaces/Units.hpp"
#include "hal/generic/TimerServiceGeneric.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "tools/simulator/model/Jk42bls01X038ed.hpp"
#include "tools/simulator/model/Model.hpp"
#include "tools/simulator/services/OnlineElectricalRls.hpp"
#include "tools/simulator/services/OnlineMechanicalRls.hpp"
#include "tools/simulator/view/gui/GuiSimulation.hpp"
#include "tools/simulator/view/gui/ParametersPanel.hpp"
#include <chrono>
#include <format>
#include <iostream>
#include <numbers>
#include <optional>
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

    try
    {
        infra::EventDispatcherWithWeakPtr::WithSize<50> eventDispatcher;

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

        constexpr uint32_t lowPriorityFrequencyHz = 10000;
        simulator::ThreePhaseMotorModel model{ simulator::JK42BLS01_X038ED::parameters, foc::Volts{ powerSupplyVoltage }, baseFrequency, std::optional<std::size_t>{} };
        if (loadTorque > 0.0f)
            model.SetLoad(foc::NewtonMeter{ loadTorque });

        foc::LowPriorityInterruptImpl lowPriorityInterrupt;
        foc::FocSpeedController controller{ model, model, foc::Ampere{ maxCurrent }, baseFrequency, lowPriorityInterrupt, hal::Hertz{ lowPriorityFrequencyHz } };

        controller.SetSpeedTunings(foc::Volts{ powerSupplyVoltage },
            controllers::PidTunings<float>{ args::get(kpSpeedArgument), args::get(kiSpeedArgument), args::get(kdSpeedArgument) });
        controller.SetCurrentTunings(foc::Volts{ powerSupplyVoltage },
            foc::IdAndIqTunings{
                { args::get(kpTorqueArgument), args::get(kiTorqueArgument), args::get(kdTorqueArgument) },
                { args::get(kpTorqueArgument), args::get(kiTorqueArgument), args::get(kdTorqueArgument) } });
        controller.SetPolePairs(simulator::JK42BLS01_X038ED::parameters.p);
        controller.SetPoint(ToRadiansPerSecond(args::get(speedSetPointArgument)));

        {
            const simulator::ParametersPanel::PidParameters pidParameters{
                .current = { args::get(kpTorqueArgument), args::get(kiTorqueArgument), args::get(kdTorqueArgument) },
                .speed = simulator::ParametersPanel::LoopPid{ args::get(kpSpeedArgument), args::get(kiSpeedArgument), args::get(kdSpeedArgument) }
            };

            const simulator::ControlPanel::SetpointConfig speedSetpointConfig{
                .label = "Speed Setpoint:",
                .unit = "RPM",
                .min = -3000,
                .max = 3000,
                .tickInterval = 500,
                .defaultValue = 0
            };

            simulator::GuiSimulation simulation{ argc, argv, model, controller, eventDispatcher, simulator::JK42BLS01_X038ED::parameters, pidParameters, speedSetpointConfig, foc::Volts{ powerSupplyVoltage } };

            hal::TimerServiceGeneric timerService;
            services::MotorAlignmentImpl alignment{ model, model };
            services::ElectricalParametersIdentificationImpl electricalIdent{ model, model, foc::Volts{ powerSupplyVoltage } };
            services::MechanicalParametersIdentificationImpl mechanicalIdent{ controller, model, model };
            const auto& motorParams = simulator::JK42BLS01_X038ED::parameters;
            const foc::NewtonMeter torqueConstant{ 1.5f * static_cast<float>(motorParams.p) * motorParams.psi_f.Value() };

            auto& gui = simulation.GetGui();

            simulator::OnlineElectricalRls electricalRls{ model, motorParams.p, baseFrequency };
            QObject::connect(&electricalRls, &simulator::OnlineElectricalRls::electricalEstimatesChanged,
                &gui, &simulator::Gui::OnElectricalRlsUpdate);

            simulator::OnlineMechanicalRls mechanicalRls{ model, electricalRls, motorParams, baseFrequency, 100 };
            QObject::connect(&mechanicalRls, &simulator::OnlineMechanicalRls::mechanicalEstimatesChanged,
                &gui, &simulator::Gui::OnMechanicalRlsUpdate);

            QObject::connect(&gui, &simulator::Gui::alignRequested, [&]()
                {
                    controller.Stop();
                    gui.SetStatus(QString::fromUtf8("Aligning\xe2\x80\xa6"));
                    alignment.ForceAlignment(motorParams.p, services::MotorAlignment::AlignmentConfig{}, [&gui](std::optional<foc::Radians> offset)
                        {
                            if (offset)
                                gui.SetAlignmentOffset(*offset);
                            gui.CalibrationFinished();
                        });
                });

            QObject::connect(&gui, &simulator::Gui::identifyElectricalRequested, [&]()
                {
                    controller.Stop();
                    gui.SetStatus(QString::fromUtf8("Identifying R/L\xe2\x80\xa6"));
                    electricalIdent.EstimateResistanceAndInductance(services::ElectricalParametersIdentification::ResistanceAndInductanceConfig{},
                        [&gui, &electricalIdent](std::optional<foc::Ohm> r, std::optional<foc::MilliHenry> l)
                        {
                            if (r && l)
                                gui.SetIdentifiedElectrical(*r, *l);

                            electricalIdent.EstimateNumberOfPolePairs(services::ElectricalParametersIdentification::PolePairsConfig{},
                                [&gui](std::optional<std::size_t> p)
                                {
                                    if (p)
                                        gui.SetIdentifiedPolePairs(*p);
                                    gui.CalibrationFinished();
                                });
                        });
                });

            QObject::connect(&gui, &simulator::Gui::identifyMechanicalRequested, [&]()
                {
                    controller.Stop();
                    gui.SetStatus(QString::fromUtf8("Identifying B/J\xe2\x80\xa6"));
                    mechanicalIdent.EstimateFrictionAndInertia(torqueConstant, motorParams.p, services::MechanicalParametersIdentification::Config{},
                        [&gui](std::optional<foc::NewtonMeterSecondPerRadian> b, std::optional<foc::NewtonMeterSecondSquared> j)
                        {
                            if (b && j)
                                gui.SetIdentifiedMechanical(*b, *j);
                            gui.CalibrationFinished();
                        });
                });

            QObject::connect(&gui, &simulator::Gui::setpointChanged, [&controller, &gui, powerSupplyVoltage, &pidParameters](int rpm)
                {
                    controller.SetPoint(ToRadiansPerSecond(static_cast<float>(rpm)));

                    controller.SetSpeedTunings(foc::Volts{ powerSupplyVoltage },
                        controllers::PidTunings<float>{ pidParameters.speed->kp, pidParameters.speed->ki, pidParameters.speed->kd });
                    controller.SetCurrentTunings(foc::Volts{ powerSupplyVoltage },
                        foc::IdAndIqTunings{
                            { pidParameters.current.kp, pidParameters.current.ki, pidParameters.current.kd },
                            { pidParameters.current.kp, pidParameters.current.ki, pidParameters.current.kd } });

                    gui.UpdatePidParameters(pidParameters);
                });

            return simulation.Run();
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
