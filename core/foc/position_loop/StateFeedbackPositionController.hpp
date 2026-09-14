#pragma once

#include "core/foc/interfaces/LoopTunings.hpp"
#include "core/foc/position_loop/PositionPlantModel.hpp"
#include "core/foc/speed_loop/SpeedPlantModel.hpp"
#include "numerical/controllers/implementations/Lqr.hpp"

namespace foc
{
    template<class Derived, std::size_t StateSize>
    class StateFeedbackPositionController
    {
    public:
        using Design = controllers::Lqr<float, StateSize, 1>;

        static bool IsDesignFeasible(const MechanicalModelParameters& parameters, const PositionLoopTunings& tunings)
        {
            return Derived::Solve(parameters, tunings).has_value();
        }

        bool Configure(const MechanicalModelParameters& motorParameters)
        {
            parameters = motorParameters;
            return Construct();
        }

        bool SetTunings(const PositionLoopTunings& controllerTunings)
        {
            tunings = controllerTunings;
            return Construct();
        }

    protected:
        void OnDesignChanged()
        {}

        bool Construct()
        {
            auto solved = Derived::Solve(parameters, tunings);

            design = solved ? *solved : Derived::Inert();
            currentPerNormalizedInput = solved ? PositionPlantModel::FromParameters(parameters).currentPerNormalizedInput : 0.0f;
            samplePeriod = solved ? OuterSamplePeriod(parameters.samplingFrequency) : 0.0f;
            static_cast<Derived*>(this)->OnDesignChanged();

            return solved.has_value();
        }

        MechanicalModelParameters parameters{};
        PositionLoopTunings tunings{};
        float currentPerNormalizedInput{ 0.0f };
        float samplePeriod{ 0.0f };
        Design design{ Derived::Inert() };
    };
}
