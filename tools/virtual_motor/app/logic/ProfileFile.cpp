#include "tools/virtual_motor/app/logic/ProfileFile.hpp"
#include "numerical/math/Math.hpp"
#include "tools/virtual_motor/app/logic/MotorText.hpp"
#include <array>
#include <charconv>
#include <format>
#include <map>
#include <span>

namespace virtual_motor::app
{
    namespace
    {
        using Words = std::vector<std::string_view>;

        struct LineError
        {
            std::string message;
        };

        Words Split(std::string_view line, char separator = ' ')
        {
            Words words;
            while (!line.empty())
            {
                const auto end = line.find(separator);
                if (const auto word = line.substr(0, end); !word.empty())
                    words.push_back(word);
                line = end == std::string_view::npos ? std::string_view{} : line.substr(end + 1);
            }
            return words;
        }

        template<class Number>
        std::optional<Number> Read(std::string_view text)
        {
            Number value{};
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc{} || end != text.data() + text.size() || !math::IsFinite(static_cast<double>(value)))
                return std::nullopt;
            return value;
        }

        std::optional<LineError> ReadFloats(const Words& words, std::size_t first, std::span<float> values, std::string_view usage)
        {
            if (words.size() != first + values.size())
                return LineError{ std::string{ usage } };
            for (std::size_t index = 0; index != values.size(); ++index)
            {
                const auto value = Read<float>(words[first + index]);
                if (!value.has_value())
                    return LineError{ std::format("\"{}\" is not a number", words[first + index]) };
                values[index] = *value;
            }
            return std::nullopt;
        }

        std::optional<LineError> ReadExternal(const Words& words, ProfileEntry& entry)
        {
            static const std::map<std::string_view, std::pair<port::TorqueForm, std::size_t>, std::less<>> forms{
                { "none", { port::TorqueForm::none, 0 } },
                { "constant", { port::TorqueForm::constant, 1 } },
                { "step", { port::TorqueForm::step, 3 } },
                { "ramp", { port::TorqueForm::ramp, 3 } },
                { "sine", { port::TorqueForm::sine, 3 } },
            };
            static const std::map<std::string_view, std::string_view, std::less<>> usages{
                { "none", "external none takes nothing more" },
                { "constant", "external constant takes the torque" },
                { "step", "external step takes from, to and delay" },
                { "ramp", "external ramp takes from, to and duration" },
                { "sine", "external sine takes offset, amplitude and frequency" },
            };
            const auto form = words.size() > 3 ? forms.find(words[3]) : forms.end();
            if (form == forms.end())
                return LineError{ "external is followed by none, constant, step, ramp or sine" };
            entry.torque.form = form->second.first;
            return ReadFloats(words, 4, std::span{ entry.torque.parameters }.first(form->second.second), usages.find(words[3])->second);
        }

        std::optional<LineError> ReadChange(const Words& words, ProfileEntry& entry)
        {
            static const std::map<std::string_view, port::MessageType, std::less<>> changes{
                { "load", port::MessageType::loadTorque },
                { "external", port::MessageType::externalTorque },
                { "current-noise", port::MessageType::currentNoise },
                { "encoder-noise", port::MessageType::encoderNoise },
            };
            const auto change = words.size() > 2 ? changes.find(words[2]) : changes.end();
            if (change == changes.end())
                return LineError{ words.size() > 2 ? std::format("unknown change \"{}\"", words[2]) : std::string{ "a change follows the time" } };
            entry.change = change->second;
            if (entry.change == port::MessageType::externalTorque)
                return ReadExternal(words, entry);
            return ReadFloats(words, 3, std::span{ &entry.value, 1 }, std::format("{} takes one value", words[2]));
        }

        std::optional<LineError> ReadLeds(std::string_view names, port::Trigger& trigger)
        {
            static const std::map<std::string_view, uint8_t, std::less<>> leds{
                { "operational", port::led::operational },
                { "warning", port::led::warning },
                { "failure", port::led::failure },
            };
            for (const auto name : Split(names, ','))
            {
                const auto led = leds.find(name);
                if (led == leds.end())
                    return LineError{ std::format("unknown LED \"{}\"", name) };
                trigger.leds = static_cast<uint8_t>(trigger.leds + led->second);
            }
            return std::nullopt;
        }

        std::optional<LineError> ReadEdge(const Words& words, port::Trigger& trigger)
        {
            const auto signal = words.size() == 4 ? Read<int>(words[2]) : std::nullopt;
            if (words.size() != 4)
                return LineError{ "an edge trigger takes the signal number and the level" };
            if (!signal.has_value() || *signal < 1 || *signal > static_cast<int>(port::signalCount))
                return LineError{ "the trigger signal is a number from 1 to 25" };
            const auto level = Read<float>(words[3]);
            if (!level.has_value())
                return LineError{ std::format("\"{}\" is not a level", words[3]) };
            trigger.signal = static_cast<uint8_t>(*signal - 1);
            trigger.level = *level;
            return std::nullopt;
        }

        std::optional<LineError> ReadTrigger(const Words& words, port::Trigger& trigger)
        {
            static const std::map<std::string_view, port::TriggerKind, std::less<>> kinds{
                { "immediately", port::TriggerKind::immediately },
                { "rising", port::TriggerKind::rising },
                { "falling", port::TriggerKind::falling },
                { "either", port::TriggerKind::either },
                { "protection", port::TriggerKind::protection },
                { "leds", port::TriggerKind::leds },
            };
            const auto kind = words.size() > 1 ? kinds.find(words[1]) : kinds.end();
            if (kind == kinds.end())
                return LineError{ "trigger is followed by immediately, rising, falling, either, protection or leds" };
            trigger = port::Trigger{ kind->second, 0, 0.0f, 0 };
            if (trigger.kind == port::TriggerKind::leds)
                return words.size() == 3 ? ReadLeds(words[2], trigger) : std::optional<LineError>{ LineError{ "trigger leds takes the LEDs, separated by commas" } };
            if (trigger.kind == port::TriggerKind::rising || trigger.kind == port::TriggerKind::falling || trigger.kind == port::TriggerKind::either)
                return ReadEdge(words, trigger);
            return words.size() == 2 ? std::nullopt : std::optional<LineError>{ LineError{ std::format("trigger {} takes nothing more", words[1]) } };
        }

        std::optional<LineError> ReadEntry(const Words& words, Profile& profile)
        {
            if (profile.entries.size() == port::maxProfileSteps)
                return LineError{ std::format("a profile holds at most {} changes", port::maxProfileSteps) };
            const auto seconds = words.size() > 1 ? Read<double>(words[1]) : std::nullopt;
            if (!seconds.has_value() || *seconds < 0.0)
                return LineError{ words.size() > 1 ? std::format("\"{}\" is not a time in seconds", words[1]) : std::string{ "at is followed by the time in seconds" } };
            if (!profile.entries.empty() && *seconds < profile.entries.back().seconds)
                return LineError{ "a change cannot come before the one listed ahead of it" };
            ProfileEntry entry{ *seconds, port::MessageType::loadTorque, 0.0f, {} };
            if (auto error = ReadChange(words, entry); error.has_value())
                return error;
            profile.entries.push_back(entry);
            return std::nullopt;
        }

        std::optional<LineError> ReadLine(const Words& words, Profile& profile)
        {
            if (words.front() == "trigger")
                return profile.entries.empty() ? ReadTrigger(words, profile.trigger) : std::optional<LineError>{ LineError{ "the trigger comes before the changes" } };
            if (words.front() == "at")
                return ReadEntry(words, profile);
            return LineError{ "a line starts with \"trigger\" or \"at\"" };
        }

        std::string ChangeText(const ProfileEntry& entry)
        {
            switch (entry.change)
            {
                case port::MessageType::loadTorque:
                    return std::format("load torque {:.4g} N·m", entry.value);
                case port::MessageType::externalTorque:
                    return "external torque " + ExternalTorqueText(entry.torque);
                case port::MessageType::currentNoise:
                    return std::format("current noise {:.4g} A", entry.value);
                default:
                    return std::format("encoder noise {:.4g} rad", entry.value);
            }
        }

        std::string RunningText(const port::ProfileState& state, const Profile* profile, uint32_t controlRateHz)
        {
            const auto rate = controlRateHz == 0 ? 1.0 : static_cast<double>(controlRateHz);
            auto text = std::format("Running: {} of {} changes applied, {:.4g} s since the trigger", state.applied, state.total, state.elapsed / rate);
            if (profile != nullptr && state.applied < profile->entries.size())
            {
                const auto& next = profile->entries[state.applied];
                text += std::format("; next at {:.4g} s: {}", next.seconds, ChangeText(next));
            }
            return text;
        }
    }

    ParsedProfile ParseProfile(std::string_view text)
    {
        Profile profile;
        std::size_t number{ 0 };
        for (const auto line : Split(text, '\n'))
        {
            ++number;
            const auto words = Split(line.substr(0, line.find('\r')));
            if (words.empty() || words.front().starts_with('#'))
                continue;
            if (const auto error = ReadLine(words, profile); error.has_value())
                return { std::nullopt, std::format("Line {}: {}", number, error->message) };
        }
        if (profile.entries.empty())
            return { std::nullopt, "The profile has no changes" };
        return { profile, {} };
    }

    port::ProfileRequest ProfileRequestFor(const Profile& profile, uint32_t controlRateHz)
    {
        port::ProfileRequest request{ profile.trigger, {}, true };
        for (const auto& entry : profile.entries)
            request.steps.push_back({ static_cast<uint32_t>(math::Round(entry.seconds * controlRateHz)), entry.change, entry.value, entry.torque });
        return request;
    }

    std::string ProfileStatusText(const std::optional<port::ProfileState>& state, const Profile* profile, uint32_t controlRateHz)
    {
        if (!state.has_value())
            return "No profile running";
        switch (state->state)
        {
            case port::ProfileStateKind::armed:
                return "Armed, waiting for the trigger";
            case port::ProfileStateKind::running:
                return RunningText(*state, profile, controlRateHz);
            case port::ProfileStateKind::finished:
                return std::format("Finished: {} changes applied, and they stay in force", state->applied);
            default:
                return "Stopped: the plant is back to its description";
        }
    }
}
