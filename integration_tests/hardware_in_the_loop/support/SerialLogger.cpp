#include "integration_tests/hardware_in_the_loop/support/SerialLogger.hpp"
#include <array>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <system_error>

namespace hil
{
    namespace
    {
        std::string FormatTimestamp(std::chrono::system_clock::time_point tp)
        {
            const auto time = std::chrono::system_clock::to_time_t(tp);
            std::tm tm{};
#ifdef _WIN32
            ::localtime_s(&tm, &time);
#else
            ::localtime_r(&time, &tm);
#endif
            std::ostringstream os;
            os << std::put_time(&tm, "%Y%m%d-%H%M%S");
            return os.str();
        }

        std::string DefaultLogRoot()
        {
            return "build/host/integration_tests/hardware_in_the_loop/logs";
        }
    }

    SerialLogger& SerialLogger::Instance()
    {
        static SerialLogger instance;
        return instance;
    }

    void SerialLogger::Configure(std::string logRoot, bool disabled)
    {
        configured = true;
        enabled = !disabled;
        configuredRoot = std::move(logRoot);
    }

    void SerialLogger::OpenSession()
    {
        if (!configured)
            Configure(DefaultLogRoot(), false);

        if (!enabled)
            return;

        const auto root = std::filesystem::path{ configuredRoot.empty() ? DefaultLogRoot() : configuredRoot };
        sessionDir = root / FormatTimestamp(std::chrono::system_clock::now());

        std::error_code ec;
        std::filesystem::create_directories(sessionDir, ec);
        if (ec)
        {
            std::cerr << "[HIL] SerialLogger: cannot create '" << sessionDir.string()
                      << "': " << ec.message() << '\n';
            enabled = false;
            return;
        }

        sessionFile.open(sessionDir / "session.log", std::ios::out | std::ios::trunc);
        if (!sessionFile)
        {
            std::cerr << "[HIL] SerialLogger: cannot open session.log in " << sessionDir.string() << '\n';
            enabled = false;
            return;
        }

        sessionStart = std::chrono::steady_clock::now();
        sessionFile << "===== HIL serial transcript (" << sessionDir.string() << ") =====\n";
        sessionFile.flush();
        std::cout << "[HIL] Serial log directory: " << sessionDir.string() << '\n';
    }

    void SerialLogger::BeginScenario()
    {
        if (!enabled)
            return;

        ++scenarioIndex;
        std::ostringstream name;
        name << "scenario-" << std::setw(3) << std::setfill('0') << scenarioIndex << ".log";

        if (scenarioFile.is_open())
            scenarioFile.close();
        scenarioFile.open(sessionDir / name.str(), std::ios::out | std::ios::trunc);
        inScenario = scenarioFile.is_open();

        std::ostringstream banner;
        banner << "===== Scenario #" << scenarioIndex << " begin =====";
        Write("HOOK", banner.str());
    }

    void SerialLogger::EndScenario(bool passed)
    {
        if (!enabled)
            return;

        std::ostringstream banner;
        banner << "===== Scenario #" << scenarioIndex << " end (" << (passed ? "PASS" : "FAIL") << ") =====";
        Write("HOOK", banner.str());

        if (scenarioFile.is_open())
            scenarioFile.close();
        inScenario = false;
    }

    void SerialLogger::CloseSession()
    {
        if (scenarioFile.is_open())
            scenarioFile.close();
        if (sessionFile.is_open())
            sessionFile.close();
    }

    void SerialLogger::RxBytes(infra::ConstByteRange data)
    {
        if (!enabled || data.empty())
            return;
        std::string_view view{ reinterpret_cast<const char*>(data.begin()), data.size() };
        Write("RX", EscapeBytes(view));
    }

    void SerialLogger::TxBytes(std::string_view data)
    {
        if (!enabled || data.empty())
            return;
        Write("TX", EscapeBytes(data));
    }

    void SerialLogger::Line(std::string_view content)
    {
        if (!enabled)
            return;
        std::ostringstream os;
        os << '"' << EscapeBytes(content) << '"';
        Write("LINE", os.str());
    }

    void SerialLogger::Prompt()
    {
        if (!enabled)
            return;
        Write("PROMPT", "");
    }

    void SerialLogger::ApiCall(std::string_view summary)
    {
        if (!enabled)
            return;
        Write("API>", summary);
    }

    void SerialLogger::ApiResult(std::string_view summary)
    {
        if (!enabled)
            return;
        Write("API<", summary);
    }

    void SerialLogger::Note(std::string_view category, std::string_view message)
    {
        if (!enabled)
            return;
        Write(category, message);
    }

    void SerialLogger::Write(std::string_view category, std::string_view payload)
    {
        if (!enabled)
            return;

        std::ostringstream line;
        line << TimestampPrefix() << ' ' << category;
        if (!payload.empty())
            line << ' ' << payload;
        line << '\n';

        const auto formatted = line.str();
        if (sessionFile.is_open())
        {
            sessionFile << formatted;
            sessionFile.flush();
        }
        if (inScenario && scenarioFile.is_open())
        {
            scenarioFile << formatted;
            scenarioFile.flush();
        }
    }

    std::string SerialLogger::TimestampPrefix()
    {
        const auto now = std::chrono::steady_clock::now();
        const auto delta = std::chrono::duration_cast<std::chrono::microseconds>(now - sessionStart).count();
        const auto seconds = delta / 1'000'000;
        const auto micros = delta % 1'000'000;

        std::ostringstream os;
        os << "[t+" << std::setw(4) << std::setfill(' ') << seconds
           << '.' << std::setw(6) << std::setfill('0') << micros << "s]";
        return os.str();
    }

    std::string SerialLogger::EscapeBytes(std::string_view bytes)
    {
        std::string out;
        out.reserve(bytes.size());

        constexpr std::array<char, 16> hex{ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

        for (unsigned char ch : bytes)
        {
            switch (ch)
            {
                case '\\':
                    out += "\\\\";
                    break;
                case '\r':
                    out += "\\r";
                    break;
                case '\n':
                    out += "\\n";
                    break;
                case '\t':
                    out += "\\t";
                    break;
                case '"':
                    out += "\\\"";
                    break;
                default:
                    if (ch >= 0x20 && ch < 0x7f)
                        out += static_cast<char>(ch);
                    else
                    {
                        out += "\\x";
                        out += hex[(ch >> 4) & 0x0f];
                        out += hex[ch & 0x0f];
                    }
            }
        }
        return out;
    }
}
