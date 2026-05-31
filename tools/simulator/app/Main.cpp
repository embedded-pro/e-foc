#include "hal/generic/TimerServiceGeneric.hpp"
#include "tools/simulator/app/ControllerKind.hpp"
#include "tools/simulator/app/LaunchDialog.hpp"
#include "tools/simulator/app/RunController.hpp"
#include "tools/simulator/view/gui/GuiSimulation.hpp"
#include <QApplication>

int main(int argc, char* argv[])
{
    simulator::GuiSimulation::Init();
    QApplication app(argc, argv);

    simulator::LaunchDialog dialog;
    if (dialog.exec() != QDialog::Accepted)
        return 0;

    [[maybe_unused]] static hal::TimerServiceGeneric timerService;

    const auto kind = dialog.Selected();
    if (!kind)
        return 0;

    switch (*kind)
    {
        case simulator::ControllerKind::Torque:
            return simulator::RunTorque();
            break;
        case simulator::ControllerKind::Speed:
            return simulator::RunSpeed();
            break;
        case simulator::ControllerKind::Position:
            return simulator::RunPosition();
            break;
        default:
            break;
    }

    return 0;
}
