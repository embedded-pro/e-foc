#include "core/supervision/I2tThermalModel.hpp"
#include "numerical/math/Math.hpp"
#include <cmath>

namespace supervision
{
    float I2tThermalModel::TimeConstant(const Rating& rating)
    {
        return rating.peakSeconds / -math::Log(1.0f - (rating.continuous * rating.continuous) / (rating.peak * rating.peak));
    }

    void I2tThermalModel::Update(float current, const Rating& rating, float stepSeconds)
    {
        squaredCurrentState += (current * current - squaredCurrentState) * (1.0f - math::Exp(-stepSeconds / TimeConstant(rating)));
    }

    void I2tThermalModel::Reset()
    {
        squaredCurrentState = 0.0f;
    }

    float I2tThermalModel::State() const
    {
        return squaredCurrentState;
    }
}
