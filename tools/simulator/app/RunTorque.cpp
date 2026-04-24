#include "core/services/alignment/MotorAlignmentImpl.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentificationImpl.hpp"
#include "foc/instantiations/FocController.hpp"
#include "foc/interfaces/Units.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "tools/simulator/app/CalibrationsWiring.hpp"
#include "tools/simulator/app/Defaults.hpp"
#include "tools/simulator/app/RunController.hpp"
#include "tools/simulator/model/Jk42bls01X038ed.hpp"
#include "tools/simulator/model/Model.hpp"
#include "tools/simulator/view/gui/ControlPanel.hpp"
#include "tools/simulator/view/gui/Gui.hpp"
#include "tools/simulator/view/gui/GuiSimulation.hpp"
#include "tools/simulator/view/gui/OnlineElectricalRls.hpp"
#include "tools/simulator/view/gui/ParametersPanel.hpp"
#include <QObject>
#include <optional>

namespace simulator
{
    int RunTorque()
    {
        infra::EventDispatcherWithWeakPtr::WithSize<50> eventDispatcher;

        const auto baseFrequency = defaults::BaseFrequency();
        const auto vdc = foc::Volts{ defaults::powerSupplyVoltageVolts };

        ThreePhaseMotorModel model{ JK42BLS01_X038ED::parameters, vdc, baseFrequency, std::optional<std::size_t>{} };
        model.SetLoad(foc::NewtonMeter{ defaults::loadTorqueNm });

        foc::FocTorqueController controller{ model, model };
        controller.SetCurrentTunings(vdc,
            foc::IdAndIqTunings{
                { defaults::currentKp, defaults::currentKi, defaults::currentKd },
                { defaults::currentKp, defaults::currentKi, defaults::currentKd } });
        controller.SetPolePairs(JK42BLS01_X038ED::parameters.p);
        controller.SetPoint(foc::IdAndIqPoint{ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

        const ParametersPanel::PidParameters pidParameters{
            .current = { defaults::currentKp, defaults::currentKi, defaults::currentKd }
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
            JK42BLS01_X038ED::parameters, pidParameters, setpointConfig, vdc };

        services::MotorAlignmentImpl alignment{ model, model };
        services::ElectricalParametersIdentificationImpl electricalIdent{ model, model, vdc };

        auto& gui = simulation.GetGui();
        gui.DisableMechanicalIdent();

        OnlineElectricalRls electricalRls{ model, JK42BLS01_X038ED::parameters.p, baseFrequency };
        QObject::connect(&electricalRls, &OnlineElectricalRls::electricalEstimatesChanged,
            &gui, &Gui::OnElectricalRlsUpdate);

        WireCommonCalibrations(gui, controller, alignment, electricalIdent, JK42BLS01_X038ED::parameters);

        QObject::connect(&gui, &Gui::setpointChanged, [&controller](int amps)
            {
                controller.SetPoint(foc::IdAndIqPoint{ foc::Ampere{ 0.0f }, foc::Ampere{ static_cast<float>(amps) } });
            });

        return simulation.Run();
    }
}
