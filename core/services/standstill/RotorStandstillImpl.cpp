#include "core/services/standstill/RotorStandstillImpl.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include <cmath>

namespace services
{
    RotorStandstillImpl::RotorStandstillImpl(drivers::Encoder& encoder)
        : encoder(encoder)
    {}

    void RotorStandstillImpl::Await(const infra::Function<void(bool atRest)>& onDone)
    {
        if (this->onDone)
        {
            onDone(false);
            return;
        }

        this->onDone = onDone;
        anchor = encoder.Read();
        pollsInBand = 0;

        pollTimer.Start(pollPeriod, [this]()
            {
                Poll();
            });
        timeoutTimer.Start(timeout, [this]()
            {
                Finish(false);
            });
    }

    void RotorStandstillImpl::Abort()
    {
        pollTimer.Cancel();
        timeoutTimer.Cancel();
        onDone = nullptr;
    }

    // Measured from where the window started rather than poll to poll, so a slow creep cannot pass one count at a time
    void RotorStandstillImpl::Poll()
    {
        const auto position = encoder.Read();

        if (std::abs(foc::detail::PositionWithWrapAround((position - anchor).Value())) < bandRadians)
            ++pollsInBand;
        else
        {
            anchor = position;
            pollsInBand = 0;
        }

        if (pollsInBand >= pollsAtRest)
            Finish(true);
    }

    void RotorStandstillImpl::Finish(bool atRest)
    {
        pollTimer.Cancel();
        timeoutTimer.Cancel();

        if (onDone)
            onDone(atRest);
    }
}
