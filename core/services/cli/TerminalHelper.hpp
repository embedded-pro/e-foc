#pragma once

#include "core/services/cli/DecimalToken.hpp"
#include "infra/stream/StringInputStream.hpp"
#include "infra/util/BoundedString.hpp"
#include <cstdint>
#include <limits>
#include <optional>

namespace services
{
    inline std::optional<float> ParseInput(const infra::BoundedConstString& data, float minValue = -std::numeric_limits<float>::infinity(), float maxValue = std::numeric_limits<float>::infinity())
    {
        if (!IsDecimalToken(data))
            return {};

        float value = 0.0f;
        infra::StringInputStream stream(data, infra::softFail);

        if (data.find('.') == infra::BoundedConstString::npos)
        {
            int32_t whole = 0;
            stream >> whole;
            value = static_cast<float>(whole);
        }
        else
            stream >> value;

        if (!stream.ErrorPolicy().Failed() && value >= minValue && value <= maxValue)
            return std::make_optional(value);
        else
            return {};
    }
}
