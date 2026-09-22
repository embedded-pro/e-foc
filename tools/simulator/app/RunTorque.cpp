#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "core/services/alignment/MotorAlignmentImpl.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentificationImpl.hpp"
#include "foc/instantiations/FocController.hpp"
#include "foc/interfaces/Units.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "motor_parameters/TeknicM2310pLn04k.hpp"
#include "tools/simulator/adapter/OnlineElectricalRls.hpp"
#include "tools/simulator/app/CalibrationsWiring.hpp"
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
    int RunTorque()
    {
        infra::EventDispatcherWithWeakPtr::WithSize<50> eventDispatcher;

        const auto baseFrequency = defaults::BaseFrequency();
        const auto vdc = foc::M_2310P_LN_04K::ratedSupply;

        foc::ThreePhaseMotorModel model{ foc::M_2310P_LN_04K::parameters, vdc, baseFrequency, std::optional<std::size_t>{} };
        model.SetLoad(foc::NewtonMeter{ defaults::loadTorqueNm });

        foc::FocTorqueController controller{ model, model, foc::Ampere{ defaults::maxCurrentAmps } };
        auto motorModel = foc::MotorModelParameters{};
        motorModel.resistance = foc::M_2310P_LN_04K::parameters.R;
        motorModel.inductance = foc::MilliHenry{ foc::M_2310P_LN_04K::parameters.Ld.Value() * 1000.0f };
        motorModel.fluxLinkage = foc::M_2310P_LN_04K::parameters.psi_f;
        motorModel.busVoltage = vdc;
        motorModel.samplingFrequency = baseFrequency;
        motorModel.polePairs = foc::M_2310P_LN_04K::parameters.p;
        controller.Configure(motorModel);
        controller.SetCurrentTunings(foc::CurrentLoopTunings{});
        controller.SetPoint(foc::IdAndIqPoint{ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

        const ParametersPanel::PidParameters pidParameters{
            .current = { defaults::currentKp, defaults::currentKi, defaults::currentKd },
            .speed = std::nullopt,
            .position = std::nullopt
        };
        const ControlPanel::SetpointConfig setpointConfig{
            .label = "Current Setpoint:",
            .unit = "A",
            .min = -15,
            .max = 15,
            .tickInterval = 1,
            .defaultValue = 0
        };

        GuiSimulation simulation{ model, controller, eventDispatcher,
            foc::M_2310P_LN_04K::parameters, pidParameters, setpointConfig, vdc };

        services::MotorAlignmentImpl alignment{ model, model };
        services::ElectricalParametersIdentificationImpl electricalIdent{ model, model, vdc };

        auto& gui = simulation.GetGui();
        gui.DisableMechanicalIdent();

        OnlineElectricalRls electricalRls{ model, foc::M_2310P_LN_04K::parameters.p, baseFrequency };
        QObject::connect(&electricalRls, &OnlineElectricalRls::electricalEstimatesChanged,
            &gui, &Gui::OnElectricalRlsUpdate);

        WireCommonCalibrations(gui, controller, alignment, electricalIdent, foc::M_2310P_LN_04K::parameters);

        QObject::connect(&gui, &Gui::setpointChanged, [&controller](int amps)
            {
                controller.SetPoint(foc::IdAndIqPoint{ foc::Ampere{ 0.0f }, foc::Ampere{ static_cast<float>(amps) } });
            });

        return simulation.Run();
    }
}
