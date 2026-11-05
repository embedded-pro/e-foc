#pragma once

#include "core/services/firmware_upgrade/PackHeaderCheck.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace services::test
{
    inline constexpr const char* testPackProductName = "e-foc test board";
    inline constexpr uint32_t testPackSignatureLength = 64;
    inline constexpr uint32_t testPackAesCounterSize = 16;

    struct TestPackDescription
    {
        std::string productName{ testPackProductName };
        std::string productVersion{ "2.3.4" };
        std::string target{ "app" };
        uint16_t headerVersion{ 1 };
        uint32_t numberOfImages{ 1 };
        uint32_t imageSize{ 300 };
        std::array<uint8_t, 3> magic{ 'U', 'P', 'H' };
        uint32_t trailing{ 0 };
    };

    template<std::size_t N>
    std::array<char, N> Text(const std::string& text)
    {
        std::array<char, N> result{};
        std::copy_n(text.begin(), std::min(text.size(), N), result.begin());
        return result;
    }

    PackHeader MakeTestPackHeader(const TestPackDescription& description = {});
    uint32_t TestPackSize(const TestPackDescription& description = {});
    std::vector<uint8_t> MakeTestPack(const TestPackDescription& description = {});
}
