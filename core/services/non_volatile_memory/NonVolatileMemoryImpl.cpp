#include "core/services/non_volatile_memory/NonVolatileMemoryImpl.hpp"

namespace services
{
    NonVolatileMemoryImpl::NonVolatileMemoryImpl(NvmRegion& calibrationRegion, NvmRegion& configRegion, NvmRegion& faultHistoryRegion, NvmRegion& learnedModelRegion)
        : calibration(calibrationRegion)
        , config(configRegion)
        , faultHistory(faultHistoryRegion)
        , learnedModel(learnedModelRegion)
    {}

    void NonVolatileMemoryImpl::SaveCalibration(const CalibrationData& data, infra::Function<void(NvmStatus)> onDone)
    {
        Store<&CalibrationRecord::Serialize>(calibration, data, onDone);
    }

    void NonVolatileMemoryImpl::LoadCalibration(CalibrationData& data, infra::Function<void(NvmStatus)> onDone)
    {
        Fetch<&CalibrationRecord::Deserialize>(calibration, data, NvmStatus::InvalidData, onDone);
    }

    void NonVolatileMemoryImpl::InvalidateCalibration(infra::Function<void(NvmStatus)> onDone)
    {
        if (!Admit(onDone))
            return;

        calibration.Erase([this](infra::ConstByteRange image)
            {
                Complete(CalibrationRecord::Deserialize(image).has_value() ? NvmStatus::WriteFailed : NvmStatus::Ok);
            });
    }

    void NonVolatileMemoryImpl::SaveConfig(const ConfigData& data, infra::Function<void(NvmStatus)> onDone)
    {
        Store<&ConfigRecord::Serialize>(config, data, onDone);
    }

    void NonVolatileMemoryImpl::LoadConfig(ConfigData& data, infra::Function<void(NvmStatus)> onDone)
    {
        Fetch<&ConfigRecord::Deserialize>(config, data, NvmStatus::Ok, onDone);
    }

    void NonVolatileMemoryImpl::SaveFaultHistory(const FaultHistoryData& history, infra::Function<void(NvmStatus)> onDone)
    {
        Store<&SerializeFaultHistory>(faultHistory, history, onDone);
    }

    void NonVolatileMemoryImpl::LoadFaultHistory(FaultHistoryData& history, infra::Function<void(NvmStatus)> onDone)
    {
        Fetch<&DeserializeFaultHistory>(faultHistory, history, NvmStatus::InvalidData, onDone);
    }

    void NonVolatileMemoryImpl::SaveLearnedModel(const LearnedModelData& model, infra::Function<void(NvmStatus)> onDone)
    {
        Store<&SerializeLearnedModel>(learnedModel, model, onDone);
    }

    void NonVolatileMemoryImpl::LoadLearnedModel(LearnedModelData& model, infra::Function<void(NvmStatus)> onDone)
    {
        Fetch<&DeserializeLearnedModel>(learnedModel, model, NvmStatus::InvalidData, onDone);
    }

    template<auto Serialize, std::size_t Size, class Data>
    void NonVolatileMemoryImpl::Store(NvmImage<Size>& image, const Data& data, const infra::Function<void(NvmStatus)>& onDone)
    {
        if (!Admit(onDone))
            return;

        Serialize(data, image.Contents());
        image.Write([this](bool verified)
            {
                Complete(verified ? NvmStatus::Ok : NvmStatus::HardwareFault);
            });
    }

    template<auto Deserialize, std::size_t Size, class Data>
    void NonVolatileMemoryImpl::Fetch(NvmImage<Size>& image, Data& data, NvmStatus statusWhenInvalid, const infra::Function<void(NvmStatus)>& onDone)
    {
        if (!Admit(onDone))
            return;

        whenInvalid = statusWhenInvalid;
        image.Read([this, &data](infra::ConstByteRange bytes)
            {
                const auto read = Deserialize(bytes);
                data = read.value_or(Data{});
                Complete(read.has_value() ? NvmStatus::Ok : whenInvalid);
            });
    }

    bool NonVolatileMemoryImpl::Admit(const infra::Function<void(NvmStatus)>& onRequestDone)
    {
        if (busy)
        {
            onRequestDone(NvmStatus::Busy);
            return false;
        }

        busy = true;
        onDone = onRequestDone;
        return true;
    }

    void NonVolatileMemoryImpl::Complete(NvmStatus status)
    {
        busy = false;
        onDone(status);
    }
}
