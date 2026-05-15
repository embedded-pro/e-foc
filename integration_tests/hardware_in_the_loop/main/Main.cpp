#include "cucumber_cpp/CucumberCpp.hpp"
#include "integration_tests/hardware_in_the_loop/support/BridgeConfig.hpp"

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
    cli.add_option("--jlink-exe", config.jlinkExe,
           "Path to the JLinkExe commander binary")
        ->required();
    cli.add_option("--jlink-device", config.jlinkDevice,
           "JLink device name (e.g. TM4C1294NCPDT)")
        ->required();
    cli.add_option("--jlink-hex", config.jlinkHex,
        "Path to the firmware image to flash before tests");
    cli.add_option("--jlink-if", config.jlinkInterface,
           "JLink target interface (SWD or JTAG)")
        ->required();
    cli.add_option("--jlink-speed-khz", config.jlinkSpeedKHz,
           "JLink interface speed in kHz")
        ->required();
    cli.add_flag("--skip-flash", config.skipFlash,
        "Skip the JLink flash step before scenarios run");

    return application.Run(argc, argv);
}
