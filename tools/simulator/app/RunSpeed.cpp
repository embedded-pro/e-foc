#include "core/foc/instantiations/LowPriorityInterruptImpl.hpp"
#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "core/services/alignment/MotorAlignmentImpl.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentificationImpl.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentificationImpl.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "foc/instantiations/FocController.hpp"
#include "foc/interfaces/Units.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "infra/util/WithSharedAccess.hpp"
#include "motor_parameters/TeknicM2310pLn04k.hpp"
#include "tools/simulator/adapter/OnlineElectricalRls.hpp"
#include "tools/simulator/adapter/OnlineMechanicalRls.hpp"
#include "tools/simulator/app/CalibrationsWiring.hpp"
#include "tools/simulator/app/Conversions.hpp"
#include "tools/simulator/app/Defaults.hpp"
#include "tools/simulator/app/RunController.hpp"
#include "tools/simulator/view/gui/ControlPanel.hpp"
#include "tools/simulator/view/gui/Gui.hpp"
#include "tools/simulator/view/gui/GuiSimulation.hpp"
#include "tools/simulator/view/gui/ParametersPanel.hpp"
#include <QObject>
#include <optional>

namespace simulator
{
    int RunSpeed()
    {
        infra::EventDispatcherWithWeakPtr::WithSize<50> eventDispatcher;

        const auto baseFrequency = defaults::BaseFrequency();
        const auto vdc = foc::M_2310P_LN_04K::ratedSupply;
        const auto& motorParams = foc::M_2310P_LN_04K::parameters;

        infra::WithSharedAccess<foc::ThreePhaseMotorModel> model{ motorParams, vdc, baseFrequency, std::optional<std::size_t>{} };
        model->SetLoad(foc::NewtonMeter{ defaults::loadTorqueNm });

        infra::WithSharedAccess<foc::LowPriorityInterruptImpl> lowPriorityInterrupt;
        foc::FocSpeedController controller{ *model, *model, foc::Ampere{ defaults::maxCurrentAmps }, baseFrequency,
            *lowPriorityInterrupt, hal::Hertz{ defaults::lowPriorityFrequencyHz } };

        auto motorModel = foc::MotorModelParameters{};
        motorModel.resistance = motorParams.R;
        motorModel.inductance = foc::MilliHenry{ motorParams.Ld.Value() * 1000.0f };
        motorModel.fluxLinkage = motorParams.psi_f;
        motorModel.busVoltage = vdc;
        motorModel.samplingFrequency = baseFrequency;
        motorModel.polePairs = motorParams.p;
        controller.Configure(motorModel);

        auto mechanics = foc::MechanicalModelParameters{};
        mechanics.inertia = foc::NewtonMeterSecondSquared{ motorParams.J.Value() };
        mechanics.viscousFriction = motorParams.B;
        mechanics.torqueConstant = foc::NewtonMeter{ 1.5f * motorParams.p * motorParams.psi_f.Value() };
        mechanics.maxCurrent = foc::Ampere{ defaults::maxCurrentAmps };
        mechanics.samplingFrequency = hal::Hertz{ defaults::lowPriorityFrequencyHz };
        controller.ConfigureMechanics(mechanics);

        controller.SetCurrentTunings(foc::CurrentLoopTunings{});
        controller.SetSpeedTunings(foc::SpeedLoopTunings{});
        controller.SetPoint(foc::RadiansPerSecond{ 0.0f });

        const ParametersPanel::PidParameters pidParameters{
            .current = { defaults::currentKp, defaults::currentKi, defaults::currentKd },
            .speed = ParametersPanel::LoopPid{ defaults::speedKp, defaults::speedKi, defaults::speedKd },
            .position = std::nullopt
        };
        const ControlPanel::SetpointConfig setpointConfig{
            .label = "Speed Setpoint:",
            .unit = "RPM",
            .min = -3000,
            .max = 3000,
            .tickInterval = 500,
            .defaultValue = 0
        };

        GuiSimulation simulation{ *model, controller, eventDispatcher,
            motorParams, pidParameters, setpointConfig, vdc };

        services::MotorAlignmentImpl alignment{ *model, *model };
        services::ElectricalParametersIdentificationImpl electricalIdent{ *model, *model, vdc };
        infra::WithSharedAccess<services::MechanicalParametersIdentificationImpl> mechanicalIdent{ controller, controller, controller, *model, *model };
        const foc::NewtonMeter torqueConstant{ 1.5f * static_cast<float>(motorParams.p) * motorParams.psi_f.Value() };

        auto& gui = simulation.GetGui();

        OnlineElectricalRls electricalRls{ *model, motorParams.p, baseFrequency };
        QObject::connect(&electricalRls, &OnlineElectricalRls::electricalEstimatesChanged,
            &gui, &Gui::OnElectricalRlsUpdate);

        OnlineMechanicalRls mechanicalRls{ *model, motorParams.p, torqueConstant, baseFrequency };
        QObject::connect(&mechanicalRls, &OnlineMechanicalRls::mechanicalEstimatesChanged,
            &gui, &Gui::OnMechanicalRlsUpdate);

        WireCommonCalibrations(gui, controller, alignment, electricalIdent, motorParams);

        QObject::connect(&gui, &Gui::identifyMechanicalRequested, [&]()
            {
                controller.Stop();
                gui.SetState(state_machine::Calibrating{ state_machine::CalibrationStep::frictionAndInertia });
                auto identificationConfig = services::MechanicalParametersIdentification::Config{};
                identificationConfig.maxCurrent = foc::Ampere{ defaults::maxCurrentAmps };
                identificationConfig.maxSpeed = foc::RadiansPerSecond{ identificationConfig.targetSpeed.Value() * 2.0f };

                mechanicalIdent->EstimateFrictionAndInertia(torqueConstant, motorParams.p,
                    identificationConfig,
                    [&gui](std::optional<foc::NewtonMeterSecondPerRadian> b, std::optional<foc::NewtonMeterSecondSquared> j)
                    {
                        if (b && j)
                            gui.SetIdentifiedMechanical(*b, *j);
                        gui.CalibrationFinished();
                        gui.SetState(state_machine::Ready{});
                    });
            });

        QObject::connect(&gui, &Gui::setpointChanged, [&controller](int rpm)
            {
                controller.SetPoint(foc::RadiansPerSecond{ static_cast<float>(rpm) * conversions::rpmToRadPerSec });
            });

        return simulation.Run();
    }
}
