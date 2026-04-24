#include "core/services/alignment/MotorAlignmentImpl.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentificationImpl.hpp"
#include "foc/implementations/LowPriorityInterruptImpl.hpp"
#include "foc/instantiations/FocController.hpp"
#include "foc/interfaces/Units.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "tools/simulator/app/CalibrationsWiring.hpp"
#include "tools/simulator/app/Conversions.hpp"
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
#include <QString>
#include <optional>

namespace simulator
{
    int RunPosition()
    {
        infra::EventDispatcherWithWeakPtr::WithSize<50> eventDispatcher;

        const auto baseFrequency = defaults::BaseFrequency();
        const auto vdc = foc::Volts{ defaults::powerSupplyVoltageVolts };
        const auto& motorParams = JK42BLS01_X038ED::parameters;

        ThreePhaseMotorModel model{ motorParams, vdc, baseFrequency, std::optional<std::size_t>{} };
        model.SetLoad(foc::NewtonMeter{ defaults::loadTorqueNm });

        foc::LowPriorityInterruptImpl lowPriorityInterrupt;
        foc::FocPositionController controller{ model, model, foc::Ampere{ defaults::maxCurrentAmps }, baseFrequency,
            lowPriorityInterrupt, hal::Hertz{ defaults::lowPriorityFrequencyHz } };

        controller.SetSpeedTunings(vdc, controllers::PidTunings<float>{ defaults::speedKp, defaults::speedKi, defaults::speedKd });
        controller.SetPositionTunings(controllers::PidTunings<float>{ defaults::positionKp, defaults::positionKi, defaults::positionKd });
        controller.SetCurrentTunings(vdc,
            foc::IdAndIqTunings{
                { defaults::currentKp, defaults::currentKi, defaults::currentKd },
                { defaults::currentKp, defaults::currentKi, defaults::currentKd } });
        controller.SetPolePairs(motorParams.p);
        controller.SetPoint(foc::Radians{ 0.0f });

        const ParametersPanel::PidParameters pidParameters{
            .current = { defaults::currentKp, defaults::currentKi, defaults::currentKd },
            .speed = ParametersPanel::LoopPid{ defaults::speedKp, defaults::speedKi, defaults::speedKd },
            .position = ParametersPanel::LoopPid{ defaults::positionKp, defaults::positionKi, defaults::positionKd }
        };
        const ControlPanel::SetpointConfig setpointConfig{
            .label = "Position Setpoint:",
            .unit = QString::fromUtf8("\xC2\xB0"),
            .min = -360,
            .max = 360,
            .tickInterval = 45,
            .defaultValue = 0
        };

        GuiSimulation simulation{ model, controller, eventDispatcher,
            motorParams, pidParameters, setpointConfig, vdc };

        services::MotorAlignmentImpl alignment{ model, model };
        services::ElectricalParametersIdentificationImpl electricalIdent{ model, model, vdc };

        auto& gui = simulation.GetGui();
        gui.DisableMechanicalIdent();

        OnlineElectricalRls electricalRls{ model, motorParams.p, baseFrequency };
        QObject::connect(&electricalRls, &OnlineElectricalRls::electricalEstimatesChanged,
            &gui, &Gui::OnElectricalRlsUpdate);

        WireCommonCalibrations(gui, controller, alignment, electricalIdent, motorParams);

        QObject::connect(&gui, &Gui::setpointChanged, [&controller](int degrees)
            {
                controller.SetPoint(foc::Radians{ static_cast<float>(degrees) * conversions::degreesToRadians });
            });

        return simulation.Run();
    }
}
