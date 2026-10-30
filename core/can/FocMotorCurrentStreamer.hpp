#pragma once

#include "can-lite/server/CanProtocolServer.hpp"
#include "core/can/FocMotorCategoryServer.hpp"
#include "core/foc/cascade/CurrentStream.hpp"
#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "hal/interfaces/Pwm.hpp"
#include "infra/timer/Timer.hpp"
#include <cstdint>
#include <optional>

namespace can
{
    class FocMotorCurrentStreamer
        : public services::CanProtocolServerObserver
    {
    public:
        static constexpr uint8_t maxFramesWaiting{ 2 };

        FocMotorCurrentStreamer(services::CanProtocolServer& protocolServer, FocMotorCategoryServer& server,
            foc::CurrentStream& stream, const services::ConfigData& config, hal::Hertz controlFrequency);

        void Online() override;
        void Offline() override;

    private:
        void Tick();
        void ApplyConfiguredRate();
        void SendWaitingSamples();
        void OnFrameDone();

        FocMotorCategoryServer& server;
        foc::CurrentStream& stream;
        const services::ConfigData& config;
        uint32_t controlFrequencyHz;
        std::optional<uint32_t> appliedRateHz;
        uint8_t framesWaiting{ 0 };
        infra::TimerRepeating timer;
    };
}
