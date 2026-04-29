#pragma once

#include "tools/hardware_bridge/client/terminal/TerminalScreen.hpp"
#include "tools/hardware_bridge/client/terminal/Vt100Parser.hpp"
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tool::terminal
{
    // Public facade. Feed bytes received from the host (e.g. a serial port);
    // the terminal updates its screen state and may queue outbound bytes
    // (e.g. responses to DSR/CPR/DA requests) which the host program can
    // retrieve via TakeOutgoing().
    class Vt100Terminal
    {
    public:
        explicit Vt100Terminal(int rows = 24, int cols = 100);

        void Feed(std::span<const uint8_t> data);
        void Feed(std::string_view data);
        void FeedByte(uint8_t b);

        const TerminalScreen& Screen() const;
        TerminalScreen& Screen();

        std::string TakeOutgoing();

        // Identity reported by DA. Default is "VT102": ESC [ ? 6 c.
        void SetDeviceAttributesResponse(std::string response);

    private:
        void OnPrint(char32_t ch);
        void OnExecute(uint8_t b);
        void OnEsc(char finalByte, char intermediate);
        void OnCsi(char finalByte, const std::vector<int>& params, bool privateMarker, char intermediate);
        void OnOsc(const std::string& payload);

        void ApplySgr(const std::vector<int>& params);
        void ApplyMode(const std::vector<int>& params, bool set, bool privateMarker);
        void ReportCursorPosition();
        void ReportDeviceStatus();
        void ReportDeviceAttributes();

        TerminalScreen screen_;
        Vt100Parser parser_;
        std::string outgoing_;
        std::string deviceAttributesResponse_{ "\x1B[?6c" };
    };
}
