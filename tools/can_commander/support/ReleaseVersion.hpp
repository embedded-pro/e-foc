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

        static std::optional<ReleaseVersion> Parse(std::string_view text);

        std::string ToString() const;

        friend auto operator<=>(const ReleaseVersion& lhs, const ReleaseVersion& rhs) = default;
    };
}
