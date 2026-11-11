#pragma once

#include "tools/virtual_motor/port/PlantPortMessages.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace virtual_motor::app
{
    struct ProfileEntry
    {
        double seconds{ 0.0 };
        port::MessageType change{ port::MessageType::loadTorque };
        float value{ 0.0f };
        port::ExternalTorque torque;
    };

    struct Profile
    {
        port::Trigger trigger;
        std::vector<ProfileEntry> entries;
    };

    struct ParsedProfile
    {
        std::optional<Profile> profile;
        std::string error;
    };

    ParsedProfile ParseProfile(std::string_view text);
    port::ProfileRequest ProfileRequestFor(const Profile& profile, uint32_t controlRateHz);
    std::string ProfileStatusText(const std::optional<port::ProfileState>& state, const Profile* profile, uint32_t controlRateHz);
}
