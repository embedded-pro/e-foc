#include "core/services/mechanical_system_ident/ObservationQueue.hpp"

namespace services
{
    void ObservationQueue::Clear()
    {
        posted = 0;
        taken = 0;
    }

    ObservationQueue::PostResult ObservationQueue::Post(const IdentificationObservation& observation)
    {
        const auto count = posted.load();
        const auto pending = count - taken.load();

        if (pending == capacity)
            return PostResult::full;

        observations[count % capacity] = observation;
        posted = count + 1;

        return pending + 1 == capacity / 2 ? PostResult::halfFull : PostResult::posted;
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
