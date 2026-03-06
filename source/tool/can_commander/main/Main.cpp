#include "source/tool/can_commander/gui/MainWindow.hpp"
#include <QApplication>
#include <cstdlib>
#include <iostream>

#ifdef __linux__
#include "source/tool/can_commander/adapter/SocketCanAdapter.hpp"
#endif

int main(int argc, char* argv[])
{
    if (qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
        qputenv("DISPLAY", "host.docker.internal:0.0");

    QApplication app(argc, argv);
    app.setApplicationName("e-foc CAN Commander");

#ifdef __linux__
    tool::SocketCanAdapter adapter;
#else
    std::cerr << "No CAN adapter available for this platform.\n"
              << "Supported platforms: Linux (SocketCAN)\n";
    return 1;
#endif

    tool::MainWindow window(adapter);
    window.show();

    return QApplication::exec();
}
