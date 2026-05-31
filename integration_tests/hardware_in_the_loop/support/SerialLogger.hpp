#pragma once

#include "infra/util/ByteRange.hpp"
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace hil
{
    class SerialLogger
    {
    public:
        static SerialLogger& Instance();

        void Configure(std::string logRoot, bool disabled);

        void OpenSession();
        void BeginScenario();
        void EndScenario(bool passed);
        void CloseSession();

        bool Enabled() const
        {
            return enabled;
        }

        const std::filesystem::path& SessionDir() const
        {
            return sessionDir;
        }

        void RxBytes(infra::ConstByteRange data);
        void TxBytes(std::string_view data);
        void Line(std::string_view content);
        void Prompt();

        void ApiCall(std::string_view summary);
        void ApiResult(std::string_view summary);
        void Note(std::string_view category, std::string_view message);

    private:
        SerialLogger() = default;

        void Write(std::string_view category, std::string_view payload);
        std::string TimestampPrefix();
        static std::string EscapeBytes(std::string_view bytes);

        bool configured{ false };
        bool enabled{ true };
        std::string configuredRoot;

        std::filesystem::path sessionDir;
        std::ofstream sessionFile;
        std::ofstream scenarioFile;
        std::size_t scenarioIndex{ 0 };
        bool inScenario{ false };

        std::chrono::steady_clock::time_point sessionStart{};
    };
}
