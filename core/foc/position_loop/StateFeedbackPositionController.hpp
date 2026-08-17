#pragma once

#include "core/foc/position_loop/PositionPlantModel.hpp"
#include "numerical/controllers/implementations/Lqr.hpp"

namespace foc
{
    template<class Derived, std::size_t StateSize>
    class StateFeedbackPositionController
    {
    public:
        using Design = controllers::Lqr<float, StateSize, 1>;

        StateFeedbackPositionController()
            : design(Derived::Inert())
        {}

        static bool IsDesignFeasible(const MechanicalModelParameters& parameters, const PositionLoopTunings& tunings)
        {
            return Derived::Solve(parameters, tunings).has_value();
        }

        void Configure(const MechanicalModelParameters& motorParameters)
        {
            parameters = motorParameters;
            Construct();
        }

        void SetTunings(const PositionLoopTunings& controllerTunings)
        {
            tunings = controllerTunings;
            Construct();
        }

    protected:
        void OnDesignChanged()
        {}

        void Construct()
        {
            auto solved = Derived::Solve(parameters, tunings);

            design = solved ? *solved : Derived::Inert();
            currentPerNormalizedInput = solved ? PositionPlantModel::FromParameters(parameters).currentPerNormalizedInput : 0.0f;
            samplePeriod = solved ? OuterSamplePeriod(parameters.samplingFrequency) : 0.0f;
            static_cast<Derived*>(this)->OnDesignChanged();
        }

        MechanicalModelParameters parameters{};
        PositionLoopTunings tunings{};
        float currentPerNormalizedInput{ 0.0f };
        float samplePeriod{ 0.0f };
        Design design;
    };
}
