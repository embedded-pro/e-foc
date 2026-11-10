#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace virtual_motor
{
    constexpr int restartExitCode{ 75 };

    std::vector<std::string> RestartArguments(const std::vector<std::string>& arguments);
    std::string WindowsQuoted(std::string_view argument);
    int Supervise(const std::vector<std::string>& arguments, const std::function<int(const std::vector<std::string>&)>& run);
}
