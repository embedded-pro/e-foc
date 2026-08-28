#include "integration_tests/software_in_the_loop/support/QemuSilFixture.hpp"
#include <chrono>
#include <cstdlib>
#include <string>

namespace
{
    void ParseDwtLine(const std::string& line, std::map<std::string, uint32_t>& out)
    {
        // Format: "DWT key=value"
        if (line.rfind("DWT ", 0) != 0)
            return;

        const std::string payload = line.substr(4);
        const auto eq = payload.find('=');
        if (eq == std::string::npos)
            return;

        const std::string key = payload.substr(0, eq);
        const std::string val = payload.substr(eq + 1);
        try
        {
            out[key] = static_cast<uint32_t>(std::stoul(val));
        }
        catch (...)
        {}
    }

    void ReadUntilDone(integration::QemuSilSession& session, std::map<std::string, uint32_t>& counts)
    {
        const std::chrono::milliseconds lineTimeout{ 10000 };
        std::string line;

        while (session.ReadLine(line, lineTimeout))
        {
            ParseDwtLine(line, counts);
            if (line == "DONE")
                break;
        }
    }
}

namespace integration
{
    QemuSilFixture::QemuSilFixture()
    {
        const char* envPath = std::getenv("QEMU_SIL_ELF");
        const std::string elfPath = (envPath != nullptr) ? envPath : QEMU_SIL_ELF_DEFAULT_PATH;

        if (!session.Start(elfPath))
            return;

        ReadUntilDone(session, cycleCounts);
        available = true;
    }

    QemuSilFixture::~QemuSilFixture()
    {
        session.Stop();
    }

    void QemuSilFixture::RunPerformanceBenchmark()
    {
        if (!available)
            return;

        session.SendLine("perf");
        ReadUntilDone(session, cycleCounts);
    }

    uint32_t QemuSilFixture::CycleCount(const std::string& key) const
    {
        const auto it = cycleCounts.find(key);
        if (it == cycleCounts.end())
            return 0;
        return it->second;
    }
}
