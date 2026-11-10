#include "tools/virtual_motor/app/logic/MotorText.hpp"
#include <cstddef>
#include <format>

namespace virtual_motor::app
{
    namespace
    {
        constexpr float lateBelow{ 0.98f };

        bool Has(uint8_t bits, uint8_t bit)
        {
            return (std::byte{ bits } & std::byte{ bit }) != std::byte{ 0 };
        }

        std::string Torque(float newtonMeter)
        {
            return std::format("{:.4f} N·m", newtonMeter);
        }
    }

    RealTime RealTimeText(const port::Status& status)
    {
        if (status.realTimeFactor >= lateBelow)
            return { std::format("{:.2f}", status.realTimeFactor), false, std::format("{} overruns", status.overruns) };

        return { std::format("{:.2f} (late)", status.realTimeFactor), true, std::format("{} overruns: the drive is running behind the wall clock", status.overruns) };
    }

    std::string LedText(uint8_t leds)
    {
        std::vector<std::string> lit;
        if (Has(leds, port::led::operational))
            lit.emplace_back("operational");
        if (Has(leds, port::led::warning))
            lit.emplace_back("warning");
        if (Has(leds, port::led::failure))
            lit.emplace_back("failure");
        if (lit.empty())
            return "none lit";

        std::string text{ lit.front() };
        for (std::size_t index = 1; index != lit.size(); ++index)
            text += ", " + lit[index];
        return text;
    }

    std::string ProtectionText(uint8_t protection)
    {
        switch (protection)
        {
            case 0:
                return "clear";
            case 1:
                return "asserted";
            default:
                return "unknown";
        }
    }

    std::string VirtualTimeText(uint64_t controlPeriods, uint32_t controlRateHz)
    {
        if (controlRateHz == 0)
            return "—";

        const auto milliseconds = controlPeriods * 1000u / controlRateHz;
        return std::format("{:02}:{:02}:{:02}.{:03}", milliseconds / 3600000u, milliseconds / 60000u % 60u, milliseconds / 1000u % 60u, milliseconds % 1000u);
    }

    std::string ExternalTorqueText(const port::ExternalTorque& torque)
    {
        const auto& p = torque.parameters;
        switch (torque.form)
        {
            case port::TorqueForm::constant:
                return "constant " + Torque(p[0]);
            case port::TorqueForm::step:
                return std::format("step from {} to {} after {:.3f} s", Torque(p[0]), Torque(p[1]), p[2]);
            case port::TorqueForm::ramp:
                return std::format("ramp from {} to {} over {:.3f} s", Torque(p[0]), Torque(p[1]), p[2]);
            case port::TorqueForm::sine:
                return std::format("sine, offset {}, amplitude {}, {:.2f} Hz", Torque(p[0]), Torque(p[1]), p[2]);
            default:
                return "none";
        }
    }

    std::vector<ActiveChange> ActiveChanges(const port::Changes& changes)
    {
        std::vector<ActiveChange> active;
        if (Has(changes.differs, port::differs::loadTorque))
            active.push_back({ ChangeKind::loadTorque, "Load torque " + Torque(changes.loadTorque) });
        if (Has(changes.differs, port::differs::externalTorque))
            active.push_back({ ChangeKind::externalTorque, "External torque: " + ExternalTorqueText(changes.externalTorque) });
        if (Has(changes.differs, port::differs::currentNoise))
            active.push_back({ ChangeKind::currentNoise, std::format("Current noise {:.4f} A", changes.currentNoise) });
        if (Has(changes.differs, port::differs::encoderNoise))
            active.push_back({ ChangeKind::encoderNoise, std::format("Encoder noise {:.5f} rad", changes.encoderNoise) });
        return active;
    }

    std::string PrintableLogLine(const std::string& line)
    {
        std::string printable;
        for (std::size_t index = 0; index < line.size(); ++index)
        {
            const auto character = static_cast<unsigned char>(line[index]);
            if (character == 0x1b && index + 1 < line.size() && line[index + 1] == '[')
            {
                index += 2;
                while (index < line.size() && (line[index] < 0x40 || line[index] > 0x7e))
                    ++index;
            }
            else if (character == '\t')
                printable += ' ';
            else if (character >= 0x20)
                printable += static_cast<char>(character);
        }
        return printable;
    }
}
