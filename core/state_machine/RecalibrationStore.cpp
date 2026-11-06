#include "core/state_machine/RecalibrationStore.hpp"
#include "infra/util/Crc.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <limits>

namespace state_machine
{
    namespace
    {
        template<class Quantity>
        std::optional<float> ValueOf(const std::optional<Quantity>& quantity)
        {
            return quantity.has_value() ? std::optional<float>{ quantity->Value() } : std::nullopt;
        }

        template<class Quantity>
        std::optional<Quantity> QuantityOf(const std::optional<float>& value)
        {
            return value.has_value() ? std::optional<Quantity>{ Quantity{ *value } } : std::nullopt;
        }
    }

    uint32_t CommissioningCheck(const MotorModel& commissioned)
    {
        infra::Crc32 crc;
        for (const auto value : { commissioned.inductance.Value(), commissioned.inertia.Value(), commissioned.viscousFriction.Value() })
        {
            const auto bytes = std::bit_cast<std::array<uint8_t, sizeof(float)>>(value);
            crc.Update(infra::MakeConstByteRange(bytes));
        }
        return crc.Result();
    }

    RecalibrationStore::RecalibrationStore(services::NonVolatileMemory& nvm, application::NvmActivity& activity)
        : nvm{ nvm }
        , activity{ activity }
    {}

    void RecalibrationStore::Start(const infra::Function<void()>& onLoaded)
    {
        this->onLoaded = onLoaded;
        Load();
    }

    StoredModel RecalibrationStore::Restored(const MotorModel& commissioned) const
    {
        if (restored.commissioning != CommissioningCheck(commissioned))
            return {};

        return StoredModel{ QuantityOf<foc::MilliHenry>(restored.inductance), QuantityOf<foc::NewtonMeterSecondSquared>(restored.inertia), QuantityOf<foc::NewtonMeterSecondPerRadian>(restored.viscousFriction) };
    }

    void RecalibrationStore::Store(const StoredModel& learned, const MotorModel& commissioned)
    {
        const services::LearnedModelData model{ CommissioningCheck(commissioned), ValueOf(learned.inductance), ValueOf(learned.inertia), ValueOf(learned.viscousFriction) };
        if (last == model)
            waiting.reset();
        else
            waiting = model;

        StoreIfDue();
    }

    void RecalibrationStore::StoreIfDue()
    {
        if (loaded && !storing && waiting.has_value() && Due())
        {
            const auto model = *waiting;
            waiting.reset();
            Write(model, false);
        }
    }

    void RecalibrationStore::Reset(const infra::Function<void(bool stored)>& onDone)
    {
        restored = {};
        resetSinceStart = true;
        waiting.reset();
        onResetRequested = onDone;
        Proceed();
    }

    uint16_t RecalibrationStore::Stores() const
    {
        return stores;
    }

    void RecalibrationStore::Load()
    {
        activity.Begin();
        nvm.LoadLearnedModel(read, [this](services::NvmStatus status)
            {
                activity.End();
                Loaded(status);
            });
    }

    void RecalibrationStore::Loaded(services::NvmStatus status)
    {
        if (status == services::NvmStatus::Busy)
        {
            retry.Start(busyRetry, [this]()
                {
                    Load();
                });
            return;
        }

        if (!resetSinceStart && status == services::NvmStatus::Ok)
        {
            restored = read;
            last = read;
        }

        loaded = true;
        onLoaded();
        Proceed();
    }

    void RecalibrationStore::Proceed()
    {
        if (storing)
            return;

        if (onResetRequested != nullptr)
            Write(services::LearnedModelData{}, true);
        else
            StoreIfDue();
    }

    void RecalibrationStore::Write(const services::LearnedModelData& model, bool reset)
    {
        storing = true;
        writing = model;
        if (reset)
            onResetWriting = onResetRequested.Clone();
        onResetRequested = nullptr;

        activity.Begin();
        nvm.SaveLearnedModel(writing, [this](services::NvmStatus status)
            {
                activity.End();
                Written(status);
            });
    }

    void RecalibrationStore::Written(services::NvmStatus status)
    {
        storing = false;
        const bool reset = onResetWriting != nullptr;

        if (status == services::NvmStatus::Busy)
        {
            if (reset)
                onResetRequested = onResetWriting.Clone();
            else if (!waiting.has_value())
                waiting = writing;
            onResetWriting = nullptr;
            retry.Start(busyRetry, [this]()
                {
                    Proceed();
                });
            return;
        }

        lastStore = infra::Now();
        if (status == services::NvmStatus::Ok)
        {
            stores = static_cast<uint16_t>(std::min<uint32_t>(stores + 1u, std::numeric_limits<uint16_t>::max()));
            last = writing;
        }
        else if (!reset && !waiting.has_value())
            waiting = writing;

        if (reset)
            onResetWriting(status == services::NvmStatus::Ok);

        Proceed();
    }

    bool RecalibrationStore::Due() const
    {
        return !lastStore.has_value() || infra::Now() - *lastStore >= storeInterval;
    }
}
