#include "cucumber_cpp/CucumberCpp.hpp"
#include "integration_tests/support/interactor/hardware/BridgeConfig.hpp"
#include "integration_tests/support/interactor/hardware/HardwareInteractor.hpp"
#include "integration_tests/support/interactor/hardware/SerialLogger.hpp"
#include "integration_tests/support/interactor/interfaces/TargetInteractor.hpp"
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

#ifdef __unix__
#include "integration_tests/support/interactor/qemu/QemuInteractor.hpp"
#endif

namespace
{
    std::string g_mode;
    std::string g_logDir{ "build/host/integration_tests/logs" };
    bool g_logDisabled{ false };

    std::string g_qemuElf;

    hil::BridgeConfig g_bridgeConfig;

    std::unique_ptr<integration::TargetInteractor> g_interactor;
}

int main(int argc, char** argv)
{
    cucumber_cpp::Application application{};
    auto& cli = application.CliParser();

    cli.add_option("--mode", g_mode, "Target mode: sil or hil")->required();

    cli.add_option("--log-dir", g_logDir, "Root directory for serial transcripts");
    cli.add_flag("--log-disabled", g_logDisabled, "Disable serial transcript logging");

    cli.add_option("--qemu-elf", g_qemuElf, "Path to the QEMU SIL firmware ELF (mode=sil)");

    cli.add_option("--bridge-host", g_bridgeConfig.host, "hardware_bridge TCP host (mode=hil)");
    cli.add_option("--serial-port", g_bridgeConfig.serialPort, "hardware_bridge serial TCP port (mode=hil)");
    cli.add_option("--can-port", g_bridgeConfig.canPort, "hardware_bridge CAN TCP port (mode=hil)");
    cli.add_option("--gdb-exe", g_bridgeConfig.gdbExe, "GDB client executable (mode=hil)");
    cli.add_option("--gdb-target", g_bridgeConfig.gdbTarget, "GDB remote target host:port (mode=hil)");
    cli.add_option("--firmware-elf", g_bridgeConfig.firmwareElf, "Firmware ELF to flash (mode=hil)");
    cli.add_flag("--skip-flash", g_bridgeConfig.skipFlash, "Skip GDB flash step (mode=hil)");

    cli.parse_complete_callback([]
        {
            if (g_mode == "sil")
            {
#ifdef __unix__
                if (g_qemuElf.empty())
                {
                    const char* env = std::getenv("QEMU_SIL_ELF");
                    g_qemuElf = (env != nullptr) ? env : QEMU_SIL_ELF_DEFAULT_PATH;
                }
                g_interactor = std::make_unique<sil::QemuInteractor>(g_qemuElf);
#else
                throw std::runtime_error{ "SIL mode requires a UNIX host" };
#endif
            }
            else if (g_mode == "hil")
            {
                hil::SerialLogger::Instance().Configure(g_logDir, g_logDisabled);
                g_interactor = std::make_unique<hil::HardwareInteractor>(g_bridgeConfig);
            }
            else
            {
                throw std::runtime_error{ "Unknown --mode: " + g_mode };
            }
            integration::TargetInteractor::SetInstance(*g_interactor);
        });

    return application.Run(argc, argv);
}
