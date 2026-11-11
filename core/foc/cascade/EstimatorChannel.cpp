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

    void EstimatorChannel::SetFluxLinkage(OnlineFluxLinkageEstimator& estimator)
    {
        fluxLinkage = &estimator;
    }

    bool EstimatorChannel::HasElectrical() const
    {
        return electrical != nullptr;
    }

    std::optional<std::pair<Ohm, MilliHenry>> EstimatorChannel::SettledElectricalEstimate() const
    {
        if (electrical == nullptr || !electrical->HasSettled())
            return std::nullopt;

        return std::make_pair(electrical->CurrentResistance(), electrical->CurrentInductance());
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

    void EstimatorChannel::UpdateFluxLinkage(float vdcInvScale, float polePairs, float outerFrequency)
    {
        if (fluxLinkage == nullptr || electrical == nullptr)
            return;

        const auto& snapshot = buffer.Acquire();
        const RadiansPerSecond electricalSpeed{ snapshot.mechanical.rotation.Value() * polePairs * outerFrequency };
        fluxLinkage->Update(QuadratureWindow{ Volts{ snapshot.meanNormalizedVq * vdcInvScale }, Ampere{ snapshot.meanIq }, Ampere{ snapshot.iqAtEnd }, Ampere{ snapshot.meanId },
            electricalSpeed, electrical->CurrentResistance(), electrical->CurrentInductance(), Volts{ vdcInvScale }, snapshot.mechanical.first });
    }
}
