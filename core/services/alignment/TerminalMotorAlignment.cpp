#include "core/services/alignment/TerminalMotorAlignment.hpp"
#include "infra/stream/StringInputStream.hpp"
#include "infra/util/Tokenizer.hpp"

namespace
{
    template<typename T>
    inline std::optional<T> ParseInput(const infra::BoundedConstString& data, T minValue = std::numeric_limits<T>::min(), T maxValue = std::numeric_limits<T>::max())
    {
        T value{};
        infra::StringInputStream stream(data, infra::softFail);
        stream >> value;

        if (!stream.ErrorPolicy().Failed() && value >= minValue && value <= maxValue)
            return std::make_optional(value);
        else
            return {};
    }
}

namespace services
{
    TerminalMotorAlignment::TerminalMotorAlignment(services::TerminalWithStorage& terminal, services::Tracer& tracer, MotorAlignment& alignment)
        : terminal(terminal)
        , tracer(tracer)
        , alignment(alignment)
    {
        terminal.AddCommand({ { "force_alignment", "fa", "Force motor alignment." },
            [this](const auto& params)
            {
                this->terminal.ProcessResult(ForceAlignment(params));
            } });
    }

    TerminalMotorAlignment::StatusWithMessage TerminalMotorAlignment::ForceAlignment(const infra::BoundedConstString& input)
    {
        infra::Tokenizer tokenizer(input, ' ');
        MotorAlignment::AlignmentConfig config;

        if (tokenizer.Size() != 1)
            return { services::TerminalWithStorage::Status::error, "invalid number of arguments" };

        auto polePair = ParseInput<uint32_t>(tokenizer.Token(0));
        if (!polePair.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value. It should be an integer." };

        alignment.ForceAlignment(*polePair, config, [this](auto result)
            {
                if (result.has_value())
                    tracer.Trace() << "Motor aligned at position (rad): " << result->Value();
                else
                    tracer.Trace() << "Motor alignment failed.";
            });
        return TerminalMotorAlignment::StatusWithMessage();
    }
}
