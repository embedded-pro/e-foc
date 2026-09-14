#pragma once

#include "core/foc/position_loop/PositionController.hpp"
#include "core/foc/position_loop/StateFeedbackPositionController.hpp"

namespace foc
{
    class LqiPositionController
        : public StateFeedbackPositionController<LqiPositionController, 3>
    {
    public:
        static constexpr PositionAlgorithm algorithm{ PositionAlgorithm::lqi };

        static std::optional<Design> Solve(const MechanicalModelParameters& parameters, const PositionLoopTunings& tunings);
        static Design Inert();

        void Reset();
        void OnDesignChanged();

        OPTIMIZE_FOR_SPEED PositionOutput Compute(const PositionControlContext& context);

    private:
        float accumulatedDeviation{ 0.0f };
    };

    static_assert(PositionController<LqiPositionController>);
}
