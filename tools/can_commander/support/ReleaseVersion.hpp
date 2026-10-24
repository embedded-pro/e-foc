#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace tool
{
    struct ReleaseVersion
    {
        uint32_t major{ 0 };
        uint32_t minor{ 0 };
        uint32_t patch{ 0 };

        // Accepts release-please tags ("v1.2.3") and CMake project versions ("1.2.3"); anything else,
        // pre-release suffixes included, is not a release this tool offers as an update.
        static std::optional<ReleaseVersion> Parse(std::string_view text);

        std::string ToString() const;

        friend bool operator==(const ReleaseVersion& lhs, const ReleaseVersion& rhs);
        friend bool operator<(const ReleaseVersion& lhs, const ReleaseVersion& rhs);
        friend bool operator>(const ReleaseVersion& lhs, const ReleaseVersion& rhs);
    };
}
