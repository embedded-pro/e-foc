#include "tools/hardware_bridge/client/gui/BridgeWindow.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include "ui/theme/Theme.hpp"
#include <QApplication>

int main(int argc, char* argv[])
{
    if (qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
        qputenv("DISPLAY", "host.docker.internal:0.0");

    QApplication app(argc, argv);
    QApplication::setApplicationName("e-foc Hardware Bridge");
    ui::backend::qt::ApplyTheme(ui::theme::Instrument());

    tool::BridgeWindow window;
    window.show();

    return QApplication::exec();
}
