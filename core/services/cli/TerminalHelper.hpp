#pragma once

#include "core/foc/interfaces/CommandLimits.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "infra/stream/StringInputStream.hpp"
#include "infra/util/Tokenizer.hpp"
#include "numerical/controllers/interfaces/PidController.hpp"
#include "services/util/TerminalWithStorage.hpp"
#include <optional>

namespace services
{
    inline std::optional<float> ParseInput(const infra::BoundedConstString& data, float minValue = -std::numeric_limits<float>::infinity(), float maxValue = std::numeric_limits<float>::infinity())
    {
        float value = 0.0f;
        infra::StringInputStream stream(data, infra::softFail);
        stream >> value;

        if (!stream.ErrorPolicy().Failed() && value >= minValue && value <= maxValue)
            return std::make_optional(value);
        else
            return {};
    }

    using StatusWithMessage = services::TerminalWithStorage::StatusWithMessage;

    inline bool Succeeded(const StatusWithMessage& status)
    {
        return status.result == services::TerminalWithStorage::Status::success;
    }

    inline StatusWithMessage ParseSingleBoundedArgument(const infra::BoundedConstString& input, float minValue, float maxValue, float& result)
    {
        infra::Tokenizer tokenizer(input, ' ');

        if (tokenizer.Size() != 1)
            return { services::TerminalWithStorage::Status::error, "invalid number of arguments." };

        auto value = ParseInput(tokenizer.Token(0), minValue, maxValue);
        if (!value.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value. It should be a float." };

        result = *value;
        return StatusWithMessage();
    }

    inline StatusWithMessage ApplySpeedBandwidth(const infra::BoundedConstString& input, foc::SpeedLoopTunable& speedLoop)
    {
        float bandwidth = 0.0f;
        auto parsed = ParseSingleBoundedArgument(input, foc::CommandLimits::minBandwidth, foc::CommandLimits::maxSpeedBandwidth, bandwidth);

        if (!Succeeded(parsed))
            return parsed;

        auto tunings = foc::SpeedLoopTunings{};
        tunings.bandwidth = bandwidth;
        speedLoop.SetSpeedTunings(tunings);
        return StatusWithMessage();
    }

    inline StatusWithMessage ParsePidTunings(const infra::BoundedConstString& input, controllers::PidTunings<float>& result)
    {
        infra::Tokenizer tokenizer(input, ' ');

        if (tokenizer.Size() != 3)
            return { services::TerminalWithStorage::Status::error, "invalid number of arguments" };

        auto kp = ParseInput(tokenizer.Token(0));
        if (!kp.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value. It should be a float." };
        auto ki = ParseInput(tokenizer.Token(1));
        if (!ki.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value. It should be a float." };
        auto kd = ParseInput(tokenizer.Token(2));
        if (!kd.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value. It should be a float." };

        result = controllers::PidTunings<float>{ *kp, *ki, *kd };
        return StatusWithMessage();
    }
}
