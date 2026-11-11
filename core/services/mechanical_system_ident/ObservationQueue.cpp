#include "core/services/mechanical_system_ident/ObservationQueue.hpp"

namespace services
{
    void ObservationQueue::Clear()
    {
        posted = 0;
        taken = 0;
    }

    bool ObservationQueue::Post(const IdentificationObservation& observation)
    {
        const auto count = posted.load();

        if (count - taken.load() == capacity)
            return false;

        observations[count % capacity] = observation;
        posted = count + 1;

        return true;
    }

    std::optional<IdentificationObservation> ObservationQueue::Take()
    {
        const auto count = taken.load();
        if (count == posted.load())
            return std::nullopt;

        const auto observation = observations[count % capacity];
        taken = count + 1;
        return observation;
    }
}
