#include "tools/hardware_bridge/client/common/ParseIPv4.hpp"
#include "tools/hardware_bridge/client/common/BridgeException.hpp"
#include <cstdio>

namespace tool
{
    services::IPv4Address ParseIPv4(const std::string& host)
    {
        unsigned int o0 = 0;
        unsigned int o1 = 0;
        unsigned int o2 = 0;
        unsigned int o3 = 0;
        if (std::sscanf(host.c_str(), "%u.%u.%u.%u", &o0, &o1, &o2, &o3) != 4 || o0 > 255 || o1 > 255 || o2 > 255 || o3 > 255)
            throw BridgeArgumentException("invalid IP address: " + host);

        return services::IPv4Address{
            static_cast<uint8_t>(o0),
            static_cast<uint8_t>(o1),
            static_cast<uint8_t>(o2),
            static_cast<uint8_t>(o3)
        };
    }
}
