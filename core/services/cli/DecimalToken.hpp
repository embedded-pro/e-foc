#pragma once

#include "infra/util/BoundedString.hpp"
#include <cstddef>

namespace services
{
    // At most 9 integer and 6 fraction digits: infra's text stream parsers wrap silently beyond that
    inline bool IsDecimalToken(const infra::BoundedConstString& token, bool allowNegative = true, bool allowFraction = true)
    {
        auto isDigit = [&token](std::size_t i)
        {
            return i != token.size() && token[i] >= '0' && token[i] <= '9';
        };

        std::size_t i = 0;
        if (allowNegative && i != token.size() && token[i] == '-')
            ++i;

        const auto integerStart = i;
        while (isDigit(i))
            ++i;
        const auto integerDigits = i - integerStart;
        if (integerDigits == 0 || integerDigits > 9)
            return false;
        if (i == token.size())
            return true;
        if (!allowFraction || token[i] != '.')
            return false;

        const auto fractionStart = ++i;
        while (isDigit(i))
            ++i;
        const auto fractionDigits = i - fractionStart;
        return i == token.size() && fractionDigits != 0 && fractionDigits <= 6;
    }
}
