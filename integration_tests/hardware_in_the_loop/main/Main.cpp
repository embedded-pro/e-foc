#include "cucumber_cpp/CucumberCpp.hpp"
#include "integration_tests/hardware_in_the_loop/support/BridgeConfig.hpp"
#include "integration_tests/hardware_in_the_loop/support/SerialLogger.hpp"
#include <string>

namespace
{
    std::string g_logDir{ "build/host/integration_tests/hardware_in_the_loop/logs" };
    bool g_logDisabled{ false };
}

int main(int argc, char** argv)
{
    cucumber_cpp::Application application{};

    auto& config = hil::MutableBridgeConfigForCli();

    auto& cli = application.CliParser();
    cli.add_option("--bridge-host", config.host,
           "hardware_bridge TCP host name or IPv4 address")
        ->required();
    cli.add_option("--serial-port", config.serialPort,
           "hardware_bridge serial TCP port")
        ->required();
    cli.add_option("--can-port", config.canPort,
           "hardware_bridge CAN TCP port")
        ->required();
    cli.add_option("--gdb-exe", config.gdbExe,
           "Path to the GDB client used for flashing (e.g. gdb-multiarch)")
        ->required();
    cli.add_option("--gdb-target", config.gdbTarget,
           "GDB remote target (host:port, e.g. host.docker.internal:2334)")
        ->required();
    cli.add_option("--firmware-elf", config.firmwareElf,
           "Path to the firmware ELF to flash before tests")
        ->expected(1);
    cli.add_flag("--skip-flash", config.skipFlash,
        "Skip the GDB flash step before scenarios run");
    cli.add_option("--log-dir", g_logDir,
        "Root directory for HIL serial transcripts; a timestamped subdirectory is created per run");
    cli.add_flag("--log-disabled", g_logDisabled,
        "Disable HIL serial transcript logging");

    cli.parse_complete_callback([]
        {
            hil::SerialLogger::Instance().Configure(g_logDir, g_logDisabled);
        });

    return application.Run(argc, argv);
}
