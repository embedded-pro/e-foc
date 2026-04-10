#include "tools/can_commander/gui/MainWindow.hpp"
#include <QApplication>
#include <QMessageBox>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#ifdef __linux__
#include "tools/can_commander/adapter/SocketCanAdapter.hpp"
#elif defined(_WIN32)
#include "tools/can_commander/adapter/WindowsCanAdapter.hpp"
#endif

int main(int argc, char* argv[])
{
    if (qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
        qputenv("DISPLAY", "host.docker.internal:0.0");

    QApplication app(argc, argv);
    QApplication::setApplicationName("e-foc CAN Commander");

#ifdef __linux__
    tool::SocketCanAdapter adapter;
#elif defined(_WIN32)
    tool::WindowsCanAdapter adapter;
#else
    std::cerr << "No CAN adapter available for this platform.\n"
              << "Supported platforms: Linux (SocketCAN), Windows (PCAN/Kvaser/CANable)\n";
    return 1;
#endif

    try
    {
        adapter.ValidateDriverAvailability();
    }
    catch (const std::runtime_error& e)
    {
        QMessageBox::critical(nullptr, "CAN Driver Not Found", QString::fromUtf8(e.what()));
        return 1;
    }

    tool::MainWindow window(adapter);
    window.show();

    return QApplication::exec();
}
