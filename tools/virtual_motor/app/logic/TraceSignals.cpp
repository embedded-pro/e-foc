#include "tools/virtual_motor/app/logic/TraceSignals.hpp"
#include <algorithm>
#include <format>

namespace virtual_motor::app
{
    const std::array<SignalInfo, port::signalCount>& Signals()
    {
        static const std::array<SignalInfo, port::signalCount> signals{ {
            { "Ia", "A", "Phase current a" },
            { "Ib", "A", "Phase current b" },
            { "Ic", "A", "Phase current c" },
            { "Ia measured", "A", "Measured phase current a" },
            { "Ib measured", "A", "Measured phase current b" },
            { "Ic measured", "A", "Measured phase current c" },
            { "Duty a", "1", "Duty applied to leg a" },
            { "Duty b", "1", "Duty applied to leg b" },
            { "Duty c", "1", "Duty applied to leg c" },
            { "Va", "V", "Leg voltage a" },
            { "Vb", "V", "Leg voltage b" },
            { "Vc", "V", "Leg voltage c" },
            { "Vα", "V", "Stator voltage α" },
            { "Vβ", "V", "Stator voltage β" },
            { "Id", "A", "True d current" },
            { "Iq", "A", "True q current" },
            { "θ", "rad", "Rotor angle, mechanical" },
            { "θ encoder", "rad", "Encoder angle" },
            { "ω", "rad/s", "Rotor speed, mechanical" },
            { "Te", "N·m", "Electrical torque" },
            { "T load", "N·m", "Load torque" },
            { "T external", "N·m", "External torque" },
            { "V bus", "V", "Bus voltage" },
            { "I bus", "A", "Bus current" },
            { "T winding", "°C", "Winding temperature" },
        } };
        return signals;
    }

    const std::vector<ViewSpec>& TraceViews()
    {
        static const std::vector<ViewSpec> views{
            { "Currents", { { "Phase currents, true and measured", "A", { 0, 1, 2, 3, 4, 5 } }, { "d and q currents", "A", { 14, 15 } } } },
            { "Voltages", { { "Duties", "1", { 6, 7, 8 } }, { "Leg voltages", "V", { 9, 10, 11 } }, { "Stator voltage", "V", { 12, 13 } } } },
            { "Mechanics", { { "Rotor speed", "rad/s", { 18 } }, { "Angle, true and encoder", "rad", { 16, 17 } }, { "Torque", "N·m", { 19, 20, 21 } } } },
            { "Supply", { { "Bus voltage", "V", { 22 } }, { "Bus current", "A", { 23 } }, { "Winding temperature", "°C", { 24 } } } },
        };
        return views;
    }

    std::optional<std::size_t> ViewOfSignal(uint8_t signal)
    {
        const auto& views = TraceViews();
        for (std::size_t view = 0; view != views.size(); ++view)
            for (const auto& panel : views[view].panels)
                if (std::ranges::find(panel.traces, signal) != panel.traces.end())
                    return view;
        return std::nullopt;
    }

    std::string SignalLabel(uint8_t signal)
    {
        const auto& info = Signals().at(signal);
        return std::format("{} [{}]", info.name, info.unit);
    }
}
