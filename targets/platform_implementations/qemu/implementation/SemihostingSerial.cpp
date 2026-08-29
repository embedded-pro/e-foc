#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "targets/platform_implementations/qemu/implementation/SemihostingSerial.hpp"
#include <cstdio>

extern "C"
{
    static int sys_write(int fd, const char* buf, int len)
    {
        (void)fd;
        for (int i = 0; i < len; ++i)
            std::putchar(static_cast<unsigned char>(buf[i]));
        return 0;
    }
}

namespace application
{
    void SemihostingSerial::SendData(infra::ConstByteRange data, infra::Function<void()> actionOnCompletion)
    {
        sys_write(0,
            reinterpret_cast<const char*>(data.begin()),
            static_cast<int>(data.size()));
        std::fflush(stdout);
        if (actionOnCompletion)
            actionOnCompletion();
    }

    void SemihostingSerial::ReceiveData(infra::Function<void(infra::ConstByteRange)> dataReceived)
    {
        onReceived = dataReceived;
    }
}
