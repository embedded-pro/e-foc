#include "source/foc/implementations/TrigonometricImpl.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace foc
{
    float TrigonometricFunctions::Sine(const float& angle) const
    {
        return FastTrigonometry::Sine(angle);
    }

    float TrigonometricFunctions::Cosine(const float& angle) const
    {
        return FastTrigonometry::Cosine(angle);
    }

    float TrigonometricFunctions::Arctangent(const float& value) const
    {
        return FastTrigonometry::Arctangent(value);
    }

    float TrigonometricFunctions::Phase(const float& real, const float& imag) const
    {
        return FastTrigonometry::Phase(real, imag);
    }
}
