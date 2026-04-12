#pragma once

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

namespace hil
{
    // HilFixture manages a serial-over-UART connection to the hardware_test
    // firmware CLI on the target board. The serial port device is read from the
    // E_FOC_HIL_SERIAL_PORT environment variable (e.g. /dev/ttyUSB0).
    //
    // This fixture is only used in HIL tests (E_FOC_BUILD_HIL_TESTS=ON).
    // It is intentionally NOT registered with ctest add_test — run manually.
    struct HilFixture
    {
        HilFixture();
        ~HilFixture();

        // Send a CLI command string and wait up to timeout for all response lines.
        // Accumulates every received line in allLines and sets lastResponse to the
        // last non-empty line. Returns true if at least one line was received.
        bool SendCommand(const std::string& command,
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 500 });

        // Last non-empty response line received from the target.
        std::string lastResponse;

        // All response lines received during the last SendCommand call.
        std::vector<std::string> allLines;

        // Elapsed time from write() to first line received during the last SendCommand call.
        std::chrono::milliseconds lastCommandDuration{ 0 };

    private:
        int serialFd{ -1 };
    };
}
