#pragma once

#include "hal_st/stm32fxxx/AdcDmaMultiChannelStm.hpp"
#include "infra/util/Function.hpp"
#include <cstddef>
#include <cstdint>
#include <utility>

namespace application
{
    template<std::size_t Channels>
    class AdcTriggeredByPwmStm
        : public hal::AdcDmaMultiChannelStmTriggeredExternally<Channels>
    {
        using Base = hal::AdcDmaMultiChannelStmTriggeredExternally<Channels>;

    public:
        template<hal::PinConfig... PinConfigs>
        AdcTriggeredByPwmStm(hal::AdcStm& adc, hal::DmaStm::ReceiveStream& receiveStream, uint32_t triggerSource, const infra::Function<void(hal::AdcMultiChannel::Samples)>& monitor, PinConfigs&&... pinConfigs)
            : Base(adc, receiveStream, triggerSource, std::forward<PinConfigs>(pinConfigs)...)
            , monitor(monitor)
        {}

        void Measure(const infra::Function<void(hal::AdcMultiChannel::Samples)>& onDone) override
        {
            this->onDone = onDone;
            Base::Measure([this](hal::AdcMultiChannel::Samples samples)
                {
                    monitor(samples);
                    this->onDone(samples);
                });
        }

    private:
        infra::Function<void(hal::AdcMultiChannel::Samples)> monitor;
        infra::Function<void(hal::AdcMultiChannel::Samples)> onDone;
    };
}
