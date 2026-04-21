#pragma once

#include "services/network/connection/Connection.hpp"
#include <string>

namespace tool
{
    services::IPv4Address ParseIPv4(const std::string& host);
}
