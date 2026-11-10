#pragma once

#include "tools/virtual_motor/port/PlantPortMessages.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace virtual_motor::app
{
    struct SignalInfo
    {
        std::string_view name;
        std::string_view unit;
        std::string_view description;
    };

    struct PanelSpec
    {
        std::string_view title;
        std::string_view unit;
        std::span<const uint8_t> traces;
    };

    struct ViewSpec
    {
        std::string_view name;
        std::span<const PanelSpec> panels;
    };

    std::span<const SignalInfo, port::signalCount> Signals();
    std::span<const ViewSpec> TraceViews();
    std::optional<std::size_t> ViewOfSignal(uint8_t signal);
    std::string SignalLabel(uint8_t signal);
}
