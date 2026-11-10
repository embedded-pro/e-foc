#pragma once

#include "tools/virtual_motor/port/PlantPortMessages.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace virtual_motor::app
{
    struct SignalInfo
    {
        std::string name;
        std::string unit;
        std::string description;
    };

    struct PanelSpec
    {
        std::string title;
        std::string unit;
        std::vector<uint8_t> traces;
    };

    struct ViewSpec
    {
        std::string name;
        std::vector<PanelSpec> panels;
    };

    const std::array<SignalInfo, port::signalCount>& Signals();
    const std::vector<ViewSpec>& TraceViews();
    std::optional<std::size_t> ViewOfSignal(uint8_t signal);
    std::string SignalLabel(uint8_t signal);
}
