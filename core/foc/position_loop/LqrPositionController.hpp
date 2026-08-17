#pragma once

#include "core/foc/position_loop/StateFeedbackPositionController.hpp"

namespace foc
{
    // State feedback on (position deviation, scaled speed). Commands current directly, so the
    // cascade bypasses the speed loop and the position law owns the whole mechanical response.
    class LqrPositionController
        : public StateFeedbackPositionController<LqrPositionController, 2>
    {
    public:
        static constexpr PositionAlgorithm algorithm{ PositionAlgorithm::lqr };

        static std::optional<Design> Solve(const MechanicalModelParameters& parameters, const PositionLoopTunings& tunings);
        static Design Inert();

        void Reset() const;

        OPTIMIZE_FOR_SPEED PositionOutput Compute(const PositionControlContext& context);
    };

    static_assert(PositionController<LqrPositionController>);
}
