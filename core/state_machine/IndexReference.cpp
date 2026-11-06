#include "core/state_machine/IndexReference.hpp"

namespace application
{
    IndexReference::IndexReference(Search search, services::Tracer& tracer)
        : search(search)
        , tracer(tracer)
    {}

    bool IndexReference::CanRestore(const services::CalibrationData& record) const
    {
        return search.has_value() && services::EncoderIndexAngle(record).has_value() && search->get().Available();
    }

    void IndexReference::Start(const services::CalibrationData& record, foc::Volts busVoltage, const infra::Function<void(bool)>& onReferenced)
    {
        onDone = onReferenced;
        ++run;

        services::IndexSearch::Config config;
        config.polePairs = record.polePairs;
        config.phaseResistance = foc::Ohm{ record.rPhase };
        config.busVoltage = busVoltage;
        config.target = foc::Radians{ record.encoderIndexAngle };

        tracer.Trace() << "[SM] Searching for the encoder index to restore the rotor reference";
        const auto token = run;
        search->get().Search(config, [this, token](std::optional<foc::Radians> reading)
            {
                if (token == run && onDone)
                    Conclude(reading);
            });
    }

    bool IndexReference::IsRunning() const
    {
        return static_cast<bool>(onDone);
    }

    void IndexReference::Abort()
    {
        if (!onDone)
            return;

        ++run;
        onDone = nullptr;
        search->get().Abort();
    }

    void IndexReference::Conclude(std::optional<foc::Radians> reading)
    {
        if (reading)
            tracer.Trace() << "[SM] Rotor reference restored from the encoder index";
        else
            tracer.Trace() << "[SM] Enable refused: no encoder index found; align the rotor or try again";

        onDone(reading.has_value());
    }
}
