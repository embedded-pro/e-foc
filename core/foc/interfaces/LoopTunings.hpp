#pragma once

namespace foc
{
    struct CurrentLoopTunings
    {
        float bandwidth{ 6283.185307f };
        float switchingGain{ 0.2f };
        float boundaryLayer{ 0.5f };
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

    struct PositionLoopTunings
    {
        float bandwidth{ 18.8495559f };
        float positionErrorWeight{ 1.0f };
        float speedErrorWeight{ 0.1f };
        float integralWeight{ 0.05f };
        float referenceTimeConstant{ 0.053051f };
    };
}
