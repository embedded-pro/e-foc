#pragma once

#include "core/foc/position_loop/PositionPlantModel.hpp"
#include "numerical/controllers/implementations/Lqr.hpp"

namespace foc
{
    // State feedback on the position-integral augmented state, which rejects the constant load
    // torque that leaves plain LQR with a standing error. Augments explicitly rather than through
    // IntegralStateFeedbackLqi so the integral row stays in the time-scaled coordinates the
    // Riccati solve needs; see documentation/design/controller-selection.md.
    class LqiPositionController
    {
    public:
        static constexpr PositionAlgorithm algorithm{ PositionAlgorithm::lqi };

        using Design = controllers::Lqr<float, 3, 1>;

        static bool IsDesignFeasible(const MechanicalModelParameters& parameters, const PositionLoopTunings& tunings);

        void Configure(const MechanicalModelParameters& motorParameters);
        void SetTunings(const PositionLoopTunings& tunings);
        void Reset();

        OPTIMIZE_FOR_SPEED PositionOutput Compute(const PositionControlContext& context);

    private:
        static std::optional<Design> Solve(const MechanicalModelParameters& parameters, const PositionLoopTunings& tunings);
        static Design Inert();
        void Construct();

        MechanicalModelParameters parameters{};
        PositionLoopTunings tunings{};
        float currentPerNormalizedInput{ 0.0f };
        float accumulatedDeviation{ 0.0f };
        Design design{ Inert() };
    };

    static_assert(PositionController<LqiPositionController>);
}
