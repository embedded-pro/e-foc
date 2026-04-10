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
