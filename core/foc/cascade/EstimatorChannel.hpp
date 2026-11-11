#pragma once

#include "core/foc/cascade/EstimatorWindowAccumulator.hpp"
#include "core/foc/interfaces/OnlineEstimators.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "infra/util/TripleBuffer.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <optional>
#include <utility>

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
        std::optional<std::pair<Ohm, MilliHenry>> SettledElectricalEstimate() const;
        void UpdateMechanical();
        void UpdateElectrical(float vdcInvScale, Ampere directCurrentReference);

    private:
        OnlineMechanicalEstimator* mechanical{ nullptr };
        OnlineElectricalEstimator* electrical{ nullptr };
        infra::TripleBuffer<EstimatorSnapshot> buffer;
    };
}
