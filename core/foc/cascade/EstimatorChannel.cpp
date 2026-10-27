#include "core/foc/cascade/EstimatorChannel.hpp"

namespace foc
{
    void EstimatorChannel::SetMechanical(OnlineMechanicalEstimator& estimator)
    {
        mechanical = &estimator;
    }

    void EstimatorChannel::SetElectrical(OnlineElectricalEstimator& estimator)
    {
        electrical = &estimator;
    }

    bool EstimatorChannel::HasElectrical() const
    {
        return electrical != nullptr;
    }

    void EstimatorChannel::UpdateMechanical()
    {
        if (mechanical == nullptr)
            return;

        mechanical->Update(buffer.Acquire().mechanical);
    }

    void EstimatorChannel::UpdateElectrical(float vdcInvScale, Ampere directCurrentReference)
    {
        if (electrical == nullptr)
            return;

        const auto& snapshot = buffer.Acquire();
        electrical->Update(ElectricalWindow{ Volts{ snapshot.meanNormalizedVd * vdcInvScale }, Ampere{ snapshot.meanId }, Ampere{ snapshot.idAtEnd }, snapshot.meanElectricalSpeedTimesIq, directCurrentReference });
    }
}
