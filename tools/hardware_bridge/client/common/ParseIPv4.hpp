#pragma once

#include "services/network/connection/Connection.hpp"
#include <optional>
#include <string>

namespace tool
{
    std::optional<services::IPv4Address> TryParseIPv4(const std::string& host);
    services::IPv4Address ParseIPv4(const std::string& host);
}
