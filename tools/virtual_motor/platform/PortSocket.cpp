#include "tools/virtual_motor/platform/PortSocket.hpp"

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <cerrno>
#include <sys/socket.h>
#endif

namespace virtual_motor
{
    std::optional<std::size_t> SendWithoutWaiting(intptr_t socket, infra::ConstByteRange bytes)
    {
#if defined(_WIN32)
        const auto native = static_cast<SOCKET>(socket);
        u_long nonBlocking{ 1 };
        ioctlsocket(native, FIONBIO, &nonBlocking);
        const auto sent = ::send(native, reinterpret_cast<const char*>(bytes.begin()), static_cast<int>(bytes.size()), 0);
        if (sent == SOCKET_ERROR)
            return WSAGetLastError() == WSAEWOULDBLOCK ? std::optional<std::size_t>{ 0 } : std::nullopt;
#else
        const auto sent = ::send(static_cast<int>(socket), bytes.begin(), bytes.size(), MSG_DONTWAIT | MSG_NOSIGNAL);
        if (sent < 0)
            return errno == EAGAIN || errno == EWOULDBLOCK ? std::optional<std::size_t>{ 0 } : std::nullopt;
#endif
        return static_cast<std::size_t>(sent);
    }
}
