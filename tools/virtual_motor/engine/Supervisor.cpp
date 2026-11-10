#include "tools/virtual_motor/engine/Supervisor.hpp"
#include <algorithm>

namespace virtual_motor
{
    namespace
    {
        std::vector<std::string> With(std::vector<std::string> arguments, const std::string& flag)
        {
            if (std::ranges::find(arguments, flag) == arguments.end())
                arguments.push_back(flag);
            return arguments;
        }

        bool NeedsQuotes(std::string_view argument)
        {
            return argument.empty() || argument.find_first_of(" \t\n\v\"") != std::string_view::npos;
        }
    }

    std::vector<std::string> RestartArguments(const std::vector<std::string>& arguments)
    {
        auto restarted = With(arguments, "--restarted");
        std::erase(restarted, "--factory-fresh");
        return restarted;
    }

    std::string WindowsQuoted(std::string_view argument)
    {
        if (!NeedsQuotes(argument))
            return std::string{ argument };

        std::string quoted{ "\"" };
        std::size_t backslashes{ 0 };
        for (const auto character : argument)
            if (character == '\\')
                ++backslashes;
            else
            {
                quoted.append(character == '"' ? backslashes * 2 + 1 : backslashes, '\\');
                quoted.push_back(character);
                backslashes = 0;
            }
        quoted.append(backslashes * 2, '\\');
        quoted.push_back('"');
        return quoted;
    }

    int Supervise(const std::vector<std::string>& arguments, const std::function<int(const std::vector<std::string>&)>& run)
    {
        auto child = With(arguments, "--supervised");
        auto code = run(child);
        while (code == restartExitCode)
        {
            child = RestartArguments(child);
            code = run(child);
        }
        return code;
    }
}
