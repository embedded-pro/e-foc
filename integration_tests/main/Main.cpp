#include "cucumber_cpp/CucumberCpp.hpp"
#include "integration_tests/support/interactor/hardware/BridgeConfig.hpp"
#include "integration_tests/support/interactor/hardware/HardwareInteractor.hpp"
#include "integration_tests/support/interactor/hardware/SerialLogger.hpp"
#include "integration_tests/support/interactor/interfaces/BootloaderTarget.hpp"
#include "integration_tests/support/interactor/interfaces/TargetInteractor.hpp"
#include "integration_tests/support/interactor/interfaces/TerminalInteractor.hpp"
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#ifdef __unix__
#include "integration_tests/support/interactor/qemu/QemuInteractor.hpp"
#include "integration_tests/support/interactor/virtual_motor/VirtualMotorInteractor.hpp"
#endif

namespace
{
    std::string g_mode;
    std::string g_logDir{ "build/host/integration_tests/logs" };
    bool g_logDisabled{ false };

    std::string g_qemuElf;
    std::string g_qemuBootloaderElf;
    std::string g_developmentKeys;
    std::string g_virtualMotor;

    hil::BridgeConfig g_bridgeConfig;

    std::unique_ptr<integration::TargetInteractor> g_interactor;

    std::string OptionOrEnvironment(const std::string& option, const char* variable, const char* fallback)
    {
        if (!option.empty())
            return option;
        const char* value = std::getenv(variable);
        return value != nullptr ? value : fallback;
    }
}

int main(int argc, char** argv)
{
    cucumber_cpp::Application application{};
    auto& cli = application.CliParser();

    cli.add_option("--mode", g_mode, "Target mode: sil, virtual or hil")->required();

    cli.add_option("--log-dir", g_logDir, "Root directory for serial transcripts");
    cli.add_flag("--log-disabled", g_logDisabled, "Disable serial transcript logging");

    cli.add_option("--qemu-elf", g_qemuElf, "Path to the QEMU SIL firmware ELF (mode=sil)");
    cli.add_option("--qemu-bootloader-elf", g_qemuBootloaderElf, "Path to the QEMU SIL bootloader ELF (mode=sil)");
    cli.add_option("--development-keys", g_developmentKeys, "Key set the emulated bootloader and the staged packs use (mode=sil)");

    cli.add_option("--virtual-motor", g_virtualMotor, "Path to the e-foc-virtual-motor program (mode=virtual)");

    cli.add_option("--bridge-host", g_bridgeConfig.host, "hardware_bridge TCP host (mode=hil)");
    cli.add_option("--serial-port", g_bridgeConfig.serialPort, "hardware_bridge serial TCP port (mode=hil)");
    cli.add_option("--can-port", g_bridgeConfig.canPort, "hardware_bridge CAN TCP port (mode=hil)");
    cli.add_option("--can-bitrate", g_bridgeConfig.canBitrate, "CAN bitrate in bit/s the target is started with; must match the bridge adapter (mode=hil)");
    cli.add_option("--gdb-exe", g_bridgeConfig.gdbExe, "GDB client executable (mode=hil)");
    cli.add_option("--gdb-target", g_bridgeConfig.gdbTarget, "GDB remote target host:port (mode=hil)");
    cli.add_option("--firmware-elf", g_bridgeConfig.firmwareElf, "Firmware ELF to flash (mode=hil)");
    cli.add_option("--bootloader-elf", g_bridgeConfig.bootloaderElf, "Bootloader ELF, with its keys injected, to program before the firmware (mode=hil)");
    cli.add_flag("--skip-flash", g_bridgeConfig.skipFlash, "Skip GDB flash step (mode=hil)");
    cli.add_flag("--restart-per-scenario", g_bridgeConfig.restartPerScenario, "Reset the target before every scenario so CAN sequence numbers and state machine start clean (mode=hil)");

    cli.parse_complete_callback([]
        {
            if (g_mode == "sil")
            {
#ifdef __unix__
                g_qemuElf = OptionOrEnvironment(g_qemuElf, "QEMU_SIL_ELF", QEMU_SIL_ELF_DEFAULT_PATH);
                g_qemuBootloaderElf = OptionOrEnvironment(g_qemuBootloaderElf, "QEMU_SIL_BOOTLOADER_ELF", QEMU_SIL_BOOTLOADER_ELF_DEFAULT_PATH);
                g_developmentKeys = OptionOrEnvironment(g_developmentKeys, "E_FOC_DEVELOPMENT_KEYS", E_FOC_DEVELOPMENT_KEYS_DEFAULT_PATH);
                auto qemu = std::make_unique<sil::QemuInteractor>(sil::QemuImages{ g_qemuElf, g_qemuBootloaderElf, g_developmentKeys });
                integration::BootloaderTarget::SetInstance(*qemu);
                g_interactor = std::move(qemu);
#else
                throw std::runtime_error{ "SIL mode requires a UNIX host" };
#endif
            }
            else if (g_mode == "virtual")
            {
#ifdef __unix__
                g_virtualMotor = OptionOrEnvironment(g_virtualMotor, "E_FOC_VIRTUAL_MOTOR", E_FOC_VIRTUAL_MOTOR_DEFAULT_PATH);
                g_interactor = std::make_unique<virtual_motor::VirtualMotorInteractor>(g_virtualMotor);
#else
                throw std::runtime_error{ "Virtual mode requires a UNIX host" };
#endif
            }
            else if (g_mode == "hil")
            {
                hil::SerialLogger::Instance().Configure(g_logDir, g_logDisabled);
                auto hardware = std::make_unique<hil::HardwareInteractor>(g_bridgeConfig);
                integration::TerminalInteractor::SetInstance(*hardware);
                g_interactor = std::move(hardware);
            }
            else
            {
                throw std::runtime_error{ "Unknown --mode: " + g_mode };
            }
            integration::TargetInteractor::SetInstance(*g_interactor);
        });

    return application.Run(argc, argv);
}
