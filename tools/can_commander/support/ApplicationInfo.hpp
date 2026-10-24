#pragma once

#include <string_view>

namespace tool::application
{
    inline constexpr std::string_view organization{ "e-foc" };
    inline constexpr std::string_view name{ "CAN Commander" };
    inline constexpr std::string_view displayName{ "e-foc CAN Commander" };
    inline constexpr std::string_view version{ E_FOC_VERSION };

    inline constexpr std::string_view repositoryUrl{ "https://github.com/embedded-pro/e-foc" };
    inline constexpr std::string_view releasesUrl{ "https://github.com/embedded-pro/e-foc/releases" };
    inline constexpr std::string_view newIssueUrl{ "https://github.com/embedded-pro/e-foc/issues/new" };
    inline constexpr std::string_view latestReleaseApiUrl{ "https://api.github.com/repos/embedded-pro/e-foc/releases/latest" };
}
