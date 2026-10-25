#pragma once

#include "core/foc/speed_loop/SpeedController.hpp"
#include "numerical/controllers/implementations/IntegralStateFeedbackLqi.hpp"
#include <optional>

namespace foc
{
    class LqiSpeedController
    {
    public:
        static constexpr SpeedAlgorithm algorithm{ SpeedAlgorithm::lqi };

        static bool IsDesignFeasible(const MechanicalModelParameters& parameters, const SpeedLoopTunings& tunings);

        bool Configure(const MechanicalModelParameters& motorParameters);
        bool SetTunings(const SpeedLoopTunings& tunings);
        void Reset();

        foc::Ampere Compute(const SpeedControlContext& context);

    private:
        using SpeedLqi = controllers::IntegralStateFeedbackLqi<float, 1, 1, 1>;

        static SpeedLqi Inert();
        static std::optional<SpeedLqi> Solve(const MechanicalModelParameters& parameters, const SpeedLoopTunings& tunings);
        bool Construct();

        MechanicalModelParameters parameters{};
        SpeedLoopTunings tunings{};
        SpeedLqi lqi{ Inert() };
    };

    static_assert(SpeedController<LqiSpeedController>);
}
