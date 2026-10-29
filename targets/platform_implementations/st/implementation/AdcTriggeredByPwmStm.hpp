#pragma once

#include "hal_st/stm32fxxx/AdcDmaMultiChannelStm.hpp"
#include "infra/util/Function.hpp"
#include <cstddef>
#include <cstdint>
#include <utility>
#include DEVICE_HEADER

namespace application
{
    // One sequence per trigger edge of the PWM timer; the monitor sees every sample, the consumer only once the monitor is done
    template<std::size_t Channels>
    class AdcTriggeredByPwmStm
        : public hal::AdcDmaMultiChannelStm<Channels, hal::AdcDmaMultiChannelStmBase::Unlimited>
    {
        using Base = hal::AdcDmaMultiChannelStm<Channels, hal::AdcDmaMultiChannelStmBase::Unlimited>;

    public:
        template<hal::PinConfig... PinConfigs>
        AdcTriggeredByPwmStm(hal::AdcStm& adc, hal::DmaStm::ReceiveStream& receiveStream, uint32_t triggerSource, const infra::Function<void(hal::AdcMultiChannel::Samples)>& monitor, PinConfigs&&... pinConfigs)
            : Base(adc, receiveStream, std::forward<PinConfigs>(pinConfigs)...)
            , monitor(monitor)
        {
            LL_ADC_REG_SetTriggerSource(adc.Handle().Instance, triggerSource);
            LL_ADC_REG_SetTriggerEdge(adc.Handle().Instance, LL_ADC_REG_TRIG_EXT_RISING);
        }

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
