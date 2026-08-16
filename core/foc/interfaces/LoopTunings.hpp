#pragma once

namespace foc
{
    struct CurrentLoopTunings
    {
        float bandwidth{ 6283.185307f };
        float switchingGain{ 1.0f };
        float boundaryLayer{ 0.2f };
        bool twoStepDeadbeat{ false };
    };

    struct SpeedLoopTunings
    {
        float bandwidth{ 188.495559f };
        float speedErrorWeight{ 1.0f };
        float integralWeight{ 0.1f };
        float observerBandwidthRatio{ 5.0f };
        float referenceTimeConstant{ 0.0053051f };
    };
}
