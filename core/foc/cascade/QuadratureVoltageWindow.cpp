#include "core/foc/cascade/QuadratureVoltageWindow.hpp"

namespace foc
{
    void QuadratureVoltageWindow::Open(uint32_t samplesPerWindow)
    {
        samples = samplesPerWindow != 0 ? samplesPerWindow : 1;
        inverseSamples = 1.0f / static_cast<float>(samples);
        sum = 0.0f;
        count = 0;
        mean = 0.0f;
        open = true;
    }

    void QuadratureVoltageWindow::Close()
    {
        open = false;
        mean = 0.0f;
    }

    void QuadratureVoltageWindow::Add(float quadratureVoltage)
    {
        sum += quadratureVoltage;

        if (++count == samples)
        {
            mean = sum * inverseSamples;
            sum = 0.0f;
            count = 0;
        }
    }
}
