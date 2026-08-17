#pragma once

#include "core/foc/position_loop/PositionPlantModel.hpp"
#include "numerical/controllers/implementations/Lqr.hpp"

namespace foc
{
    // State feedback on (position deviation, scaled speed). Commands current directly, so the
    // cascade bypasses the speed loop and the position law owns the whole mechanical response.
    class LqrPositionController
    {
    public:
        static constexpr PositionAlgorithm algorithm{ PositionAlgorithm::lqr };

        using Design = controllers::Lqr<float, 2, 1>;

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
        float samplePeriod{ 0.0f };
        Design design{ Inert() };
    };

    static_assert(PositionController<LqrPositionController>);
}
