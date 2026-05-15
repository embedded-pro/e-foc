#include "integration_tests/hardware_in_the_loop/support/JLinkFlasher.hpp"
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace hil
{
    namespace
    {
        struct ScopedTempFile
        {
            std::string path;

            ScopedTempFile()
            {
                char buffer[] = "/tmp/efoc_hil_jlinkXXXXXX";
                const int fd = ::mkstemp(buffer);
                if (fd < 0)
                    throw std::runtime_error{ std::string{ "mkstemp failed: " } + std::strerror(errno) };
                ::close(fd);
                path = buffer;
            }

            ~ScopedTempFile()
            {
                if (!path.empty())
                    ::unlink(path.c_str());
            }
        };

        void WriteFile(const std::string& path, const std::string& content)
        {
            std::FILE* file = std::fopen(path.c_str(), "w");
            if (file == nullptr)
                throw std::runtime_error{ "Failed to open " + path + " for writing" };
            const auto written = std::fwrite(content.data(), 1, content.size(), file);
            std::fclose(file);
            if (written != content.size())
                throw std::runtime_error{ "Short write to " + path };
        }

        std::string BuildScript(const BridgeConfig& cfg)
        {
            std::string script;
            script += "device " + cfg.jlinkDevice + "\n";
            script += "si " + std::string{ cfg.jlinkInterface == "JTAG" ? "1" : "2" } + "\n";
            script += "speed " + std::to_string(cfg.jlinkSpeedKHz) + "\n";
            script += "connect\n";
            script += "loadfile " + cfg.jlinkHex + "\n";
            script += "r\n";
            script += "g\n";
            script += "qc\n";
            return script;
        }

        int RunAndCapture(const std::string& command, std::string& outOutput)
        {
            outOutput.clear();
            std::FILE* pipe = ::popen(command.c_str(), "r");
            if (pipe == nullptr)
                throw std::runtime_error{ "popen failed for: " + command };

            std::array<char, 256> buffer{};
            while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
                outOutput += buffer.data();

            const int rc = ::pclose(pipe);
            if (rc < 0)
                throw std::runtime_error{ "pclose failed for: " + command };

            if (WIFEXITED(rc))
                return WEXITSTATUS(rc);
            return -1;
        }
    }

    JLinkFlasher::JLinkFlasher(const BridgeConfig& config)
        : config(config)
    {}

    void JLinkFlasher::Flash() const
    {
        if (config.skipFlash)
        {
            std::cout << "[HIL] Flash skipped (E_FOC_HIL_SKIP_FLASH set)\n";
            return;
        }

        if (config.jlinkDevice.empty() || config.jlinkHex.empty())
        {
            std::cout << "[HIL] Flash skipped: jlinkDevice or jlinkHex unresolved\n";
            return;
        }

        ScopedTempFile script;
        WriteFile(script.path, BuildScript(config));

        const std::string command = config.jlinkExe +
                                    " -CommanderScript " + script.path +
                                    " -ExitOnError 1 -NoGui 1";

        std::cout << "[HIL] Flashing target: " << command << "\n";

        std::string output;
        const int exitCode = RunAndCapture(command, output);
        std::cout << output;

        if (exitCode != 0)
            throw std::runtime_error{ "JLinkExe flash failed (exit " +
                                      std::to_string(exitCode) + ")" };

        std::this_thread::sleep_for(config.postFlashDelay);
    }
}
