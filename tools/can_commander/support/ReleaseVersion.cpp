#include "tools/can_commander/support/ReleaseVersion.hpp"
#include <charconv>
#include <format>

namespace tool
{
    namespace
    {
        std::optional<uint32_t> ParseComponent(std::string_view& text, bool last)
        {
            uint32_t value{ 0 };
            const auto* begin = text.data();
            const auto* end = text.data() + text.size();
            const auto [next, error] = std::from_chars(begin, end, value);

            if (error != std::errc{} || next == begin)
                return std::nullopt;

            if (next - begin > 1 && *begin == '0')
                return std::nullopt;

            text.remove_prefix(static_cast<std::size_t>(next - begin));

            if (last)
                return text.empty() ? std::optional{ value } : std::nullopt;

            if (text.empty() || text.front() != '.')
                return std::nullopt;

            text.remove_prefix(1);
            return value;
        }
    }

    std::optional<ReleaseVersion> ReleaseVersion::Parse(std::string_view text)
    {
        if (!text.empty() && (text.front() == 'v' || text.front() == 'V'))
            text.remove_prefix(1);

        const auto major = ParseComponent(text, false);
        if (!major.has_value())
            return std::nullopt;

        const auto minor = ParseComponent(text, false);
        if (!minor.has_value())
            return std::nullopt;

        const auto patch = ParseComponent(text, true);
        if (!patch.has_value())
            return std::nullopt;

        return ReleaseVersion{ *major, *minor, *patch };
    }

    std::string ReleaseVersion::ToString() const
    {
        return std::format("{}.{}.{}", major, minor, patch);
    }

    bool operator==(const ReleaseVersion& lhs, const ReleaseVersion& rhs) = default;
    std::strong_ordering operator<=>(const ReleaseVersion& lhs, const ReleaseVersion& rhs) = default;
}
