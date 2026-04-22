#include "tools/hardware_bridge/client/gui/BridgeWindow.hpp"
#include <QApplication>

int main(int argc, char* argv[])
{
    if (qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
        qputenv("DISPLAY", "host.docker.internal:0.0");

    QApplication app(argc, argv);
    QApplication::setApplicationName("e-foc Hardware Bridge");

    tool::BridgeWindow window;
    window.show();

    return QApplication::exec();
}
