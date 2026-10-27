#pragma once

#include "core/foc/cascade/EstimatorWindowAccumulator.hpp"
#include "core/foc/cascade/TripleBuffer.hpp"
#include "core/foc/interfaces/OnlineEstimators.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "numerical/math/CompilerOptimizations.hpp"

namespace foc
{
    class EstimatorChannel
    {
    public:
        ALWAYS_INLINE_HOT void Publish(const EstimatorSnapshot& snapshot)
        {
            buffer.Publish(snapshot);
        }

        void SetMechanical(OnlineMechanicalEstimator& estimator);
        void SetElectrical(OnlineElectricalEstimator& estimator);
        bool HasElectrical() const;
        void UpdateMechanical();
        void UpdateElectrical(float vdcInvScale, Ampere directCurrentReference);

    private:
        OnlineMechanicalEstimator* mechanical{ nullptr };
        OnlineElectricalEstimator* electrical{ nullptr };
        TripleBuffer<EstimatorSnapshot> buffer;
    };
}
