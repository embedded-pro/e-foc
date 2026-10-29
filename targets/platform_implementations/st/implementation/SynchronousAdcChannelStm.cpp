#include "targets/platform_implementations/st/implementation/SynchronousAdcChannelStm.hpp"
#include "generated/stm32fxxx/PeripheralTable.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace
{
    constexpr uint32_t conversionTimeoutMs = 2;
}

namespace application
{
    SynchronousAdcChannelStm::SynchronousAdcChannelStm(uint8_t oneBasedAdcIndex, hal::GpioPinStm& pin, uint32_t samplingTime)
        : index(static_cast<uint8_t>(oneBasedAdcIndex - 1))
    {
        hal::EnableClockAdc(index);

        handle.Instance = hal::peripheralAdc[index];
        handle.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
        handle.Init.Resolution = ADC_RESOLUTION_12B;
        handle.Init.DataAlign = ADC_DATAALIGN_RIGHT;
        handle.Init.GainCompensation = 0;
        handle.Init.ScanConvMode = ADC_SCAN_DISABLE;
        handle.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
        handle.Init.LowPowerAutoWait = DISABLE;
        handle.Init.ContinuousConvMode = DISABLE;
        handle.Init.NbrOfConversion = 1;
        handle.Init.DiscontinuousConvMode = DISABLE;
        handle.Init.ExternalTrigConv = ADC_SOFTWARE_START;
        handle.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
        handle.Init.DMAContinuousRequests = DISABLE;
        handle.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
        handle.Init.OversamplingMode = ENABLE;
        handle.Init.Oversampling.Ratio = ADC_OVERSAMPLING_RATIO_8;
        handle.Init.Oversampling.RightBitShift = ADC_RIGHTBITSHIFT_3;
        handle.Init.Oversampling.TriggeredMode = ADC_TRIGGEREDMODE_SINGLE_TRIGGER;
        handle.Init.Oversampling.OversamplingStopReset = ADC_REGOVERSAMPLING_CONTINUED_MODE;

        auto result = HAL_ADC_Init(&handle);
        really_assert(result == HAL_OK);

        result = HAL_ADCEx_Calibration_Start(&handle, ADC_SINGLE_ENDED);
        really_assert(result == HAL_OK);

        ADC_ChannelConfTypeDef channel{};
        channel.Channel = __LL_ADC_DECIMAL_NB_TO_CHANNEL(pin.AdcChannel(oneBasedAdcIndex));
        channel.Rank = ADC_REGULAR_RANK_1;
        channel.SamplingTime = samplingTime;
        channel.SingleDiff = ADC_SINGLE_ENDED;
        channel.OffsetNumber = ADC_OFFSET_NONE;
        channel.Offset = 0;

        result = HAL_ADC_ConfigChannel(&handle, &channel);
        really_assert(result == HAL_OK);
    }

    SynchronousAdcChannelStm::~SynchronousAdcChannelStm()
    {
        HAL_ADC_DeInit(&handle);
        hal::DisableClockAdc(index);
    }

    uint16_t SynchronousAdcChannelStm::Measure()
    {
        if (HAL_ADC_Start(&handle) != HAL_OK)
            return 0;

        const auto converted = HAL_ADC_PollForConversion(&handle, conversionTimeoutMs) == HAL_OK;
        const auto value = converted ? static_cast<uint16_t>(HAL_ADC_GetValue(&handle)) : uint16_t{ 0 };
        HAL_ADC_Stop(&handle);

        return value;
    }
}
