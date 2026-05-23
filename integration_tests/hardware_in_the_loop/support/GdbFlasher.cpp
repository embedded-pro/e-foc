#include "integration_tests/hardware_in_the_loop/support/GdbFlasher.hpp"
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
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
                char buffer[] = "/tmp/efoc_hil_gdbXXXXXX";
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
            script += "set pagination off\n";
            script += "set confirm off\n";
            script += "set print thread-events off\n";
            script += "target extended-remote " + cfg.gdbTarget + "\n";
            script += "monitor reset\n";
            script += "monitor halt\n";
            script += "load\n";
            script += "compare-sections\n";
            script += "monitor reset\n";
            script += "monitor go\n";
            script += "disconnect\n";
            script += "quit\n";
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

    GdbFlasher::GdbFlasher(const BridgeConfig& config)
        : config(config)
    {}

    void GdbFlasher::Flash() const
    {
        if (config.skipFlash)
        {
            std::cout << "[HIL] Flash skipped (--skip-flash)\n";
            return;
        }

        if (config.gdbExe.empty() || config.gdbTarget.empty() || config.firmwareElf.empty())
            throw std::runtime_error{ "GdbFlasher: gdbExe, gdbTarget and firmwareElf must be set" };

        ScopedTempFile script;
        WriteFile(script.path, BuildScript(config));

        const std::string command = config.gdbExe +
                                    " --batch --nx" +
                                    " -x " + script.path + " " +
                                    config.firmwareElf + " 2>&1";

        std::cout << "[HIL] Flashing target via GDB: " << command << "\n";

        std::string output;
        const int exitCode = RunAndCapture(command, output);
        std::cout << output;

        if (exitCode != 0)
            throw std::runtime_error{ "GDB flash failed (exit " +
                                      std::to_string(exitCode) + ")" };

        std::this_thread::sleep_for(config.postFlashDelay);
    }
}
