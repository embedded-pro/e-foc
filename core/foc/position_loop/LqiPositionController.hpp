#pragma once

#include "core/foc/position_loop/StateFeedbackPositionController.hpp"

namespace foc
{
    // State feedback on the position-integral augmented state, which rejects the constant load
    // torque that leaves plain LQR with a standing error. Augments explicitly rather than through
    // IntegralStateFeedbackLqi so the integral row stays in the time-scaled coordinates the
    // Riccati solve needs; see documentation/design/controller-selection.md.
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
