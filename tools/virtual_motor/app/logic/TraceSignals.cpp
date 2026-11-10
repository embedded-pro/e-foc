#include "tools/virtual_motor/app/logic/TraceSignals.hpp"
#include <algorithm>
#include <format>

namespace virtual_motor::app
{
    namespace
    {
        constexpr std::array<SignalInfo, port::signalCount> signals{ {
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

        constexpr std::array<uint8_t, 6> phaseCurrents{ 0, 1, 2, 3, 4, 5 };
        constexpr std::array<uint8_t, 2> dqCurrents{ 14, 15 };
        constexpr std::array<uint8_t, 3> duties{ 6, 7, 8 };
        constexpr std::array<uint8_t, 3> legVoltages{ 9, 10, 11 };
        constexpr std::array<uint8_t, 2> statorVoltage{ 12, 13 };
        constexpr std::array<uint8_t, 1> speed{ 18 };
        constexpr std::array<uint8_t, 2> angles{ 16, 17 };
        constexpr std::array<uint8_t, 3> torques{ 19, 20, 21 };
        constexpr std::array<uint8_t, 1> busVoltage{ 22 };
        constexpr std::array<uint8_t, 1> busCurrent{ 23 };
        constexpr std::array<uint8_t, 1> temperature{ 24 };

        constexpr std::array<PanelSpec, 2> currentPanels{ {
            { "Phase currents, true and measured", "A", phaseCurrents },
            { "d and q currents", "A", dqCurrents },
        } };
        constexpr std::array<PanelSpec, 3> voltagePanels{ {
            { "Duties", "1", duties },
            { "Leg voltages", "V", legVoltages },
            { "Stator voltage", "V", statorVoltage },
        } };
        constexpr std::array<PanelSpec, 3> mechanicsPanels{ {
            { "Rotor speed", "rad/s", speed },
            { "Angle, true and encoder", "rad", angles },
            { "Torque", "N·m", torques },
        } };
        constexpr std::array<PanelSpec, 3> supplyPanels{ {
            { "Bus voltage", "V", busVoltage },
            { "Bus current", "A", busCurrent },
            { "Winding temperature", "°C", temperature },
        } };

        constexpr std::array<ViewSpec, 4> views{ {
            { "Currents", currentPanels },
            { "Voltages", voltagePanels },
            { "Mechanics", mechanicsPanels },
            { "Supply", supplyPanels },
        } };
    }

    std::span<const SignalInfo, port::signalCount> Signals()
    {
        return signals;
    }

    std::span<const ViewSpec> TraceViews()
    {
        return views;
    }

    std::optional<std::size_t> ViewOfSignal(uint8_t signal)
    {
        const auto shows = [signal](const ViewSpec& view)
        {
            return std::ranges::any_of(view.panels, [signal](const PanelSpec& panel)
                {
                    return std::ranges::find(panel.traces, signal) != panel.traces.end();
                });
        };
        const auto view = std::ranges::find_if(views, shows);
        return view != views.end() ? std::optional<std::size_t>{ static_cast<std::size_t>(view - views.begin()) } : std::nullopt;
    }

    std::string SignalLabel(uint8_t signal)
    {
        const auto& info = signals.at(signal);
        return std::format("{} [{}]", info.name, info.unit);
    }
}
