#include "integration_tests/hardware_in_the_loop/support/HilFixture.hpp"
#include <cassert>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <termios.h>
#include <thread>
#include <unistd.h>

namespace hil
{
    namespace
    {
        constexpr const char* serialPortEnvVar = "E_FOC_HIL_SERIAL_PORT";
        constexpr speed_t baudRate = B115200;
    }

    HilFixture::HilFixture()
    {
        const char* port = std::getenv(serialPortEnvVar);
        if (port == nullptr || port[0] == '\0')
        {
            throw std::runtime_error(
                std::string{ "HIL serial port not configured. "
                             "Set environment variable " } +
                serialPortEnvVar);
        }

        serialFd = ::open(port, O_RDWR | O_NOCTTY | O_SYNC);
        if (serialFd < 0)
        {
            throw std::runtime_error(
                std::string{ "Failed to open HIL serial port '" } + port +
                "': " + std::strerror(errno));
        }

        termios tty{};
        if (::tcgetattr(serialFd, &tty) != 0)
        {
            ::close(serialFd);
            throw std::runtime_error("tcgetattr failed");
        }

        ::cfsetospeed(&tty, baudRate);
        ::cfsetispeed(&tty, baudRate);
        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
        tty.c_iflag &= ~IGNBRK;
        tty.c_lflag = 0;
        tty.c_oflag = 0;
        tty.c_cc[VMIN] = 0;
        tty.c_cc[VTIME] = 1;
        tty.c_cflag |= (CLOCAL | CREAD);
        tty.c_cflag &= ~(PARENB | PARODD);
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;

        if (::tcsetattr(serialFd, TCSANOW, &tty) != 0)
        {
            ::close(serialFd);
            throw std::runtime_error("tcsetattr failed");
        }
    }

    HilFixture::~HilFixture()
    {
        if (serialFd >= 0)
        {
            ::close(serialFd);
        }
    }

    bool HilFixture::SendCommand(const std::string& command,
        std::chrono::milliseconds timeout)
    {
        lastResponse.clear();
        allLines.clear();

        std::string cmd = command + "\r\n";
        const auto written = ::write(serialFd, cmd.data(), cmd.size());
        if (written < 0)
        {
            return false;
        }

        const auto start = std::chrono::steady_clock::now();
        const auto deadline = start + timeout;
        bool firstLineReceived{ false };
        std::string buffer;
        char ch{ 0 };

        while (std::chrono::steady_clock::now() < deadline)
        {
            const auto n = ::read(serialFd, &ch, 1);
            if (n > 0)
            {
                if (ch == '\n')
                {
                    if (!buffer.empty() && buffer.back() == '\r')
                    {
                        buffer.pop_back();
                    }
                    if (!buffer.empty())
                    {
                        if (!firstLineReceived)
                        {
                            lastCommandDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - start);
                            firstLineReceived = true;
                        }
                        allLines.push_back(buffer);
                        lastResponse = buffer;
                    }
                    buffer.clear();
                }
                else
                {
                    buffer += ch;
                }
            }
        }

        return !allLines.empty();
    }
}
