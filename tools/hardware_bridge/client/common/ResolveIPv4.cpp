#include "tools/hardware_bridge/client/common/ResolveIPv4.hpp"
#include "tools/hardware_bridge/client/common/BridgeException.hpp"
#include "tools/hardware_bridge/client/common/ParseIPv4.hpp"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#endif

namespace tool
{
    services::IPv4Address ResolveIPv4(const std::string& host)
    {
        try
        {
            return ParseIPv4(host);
        }
        catch (const BridgeArgumentException&)
        {
        }

        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        addrinfo* result = nullptr;
        const int rc = ::getaddrinfo(host.c_str(), nullptr, &hints, &result);
        if (rc != 0 || result == nullptr)
        {
            if (result != nullptr)
                ::freeaddrinfo(result);
            throw BridgeArgumentException("cannot resolve host '" + host + "'");
        }

        const auto* in = reinterpret_cast<const sockaddr_in*>(result->ai_addr);
        const uint32_t addr = ntohl(in->sin_addr.s_addr);
        ::freeaddrinfo(result);

        return services::IPv4Address{
            static_cast<uint8_t>((addr >> 24) & 0xff),
            static_cast<uint8_t>((addr >> 16) & 0xff),
            static_cast<uint8_t>((addr >> 8) & 0xff),
            static_cast<uint8_t>(addr & 0xff)
        };
    }
}
