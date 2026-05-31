#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include <cstdint>

namespace simulator::defaults
{
    constexpr float powerSupplyVoltageVolts = 24.0f;
    constexpr float maxCurrentAmps = 15.0f;
    constexpr float loadTorqueNm = 0.02f;
    constexpr int timeStepMicroseconds = 10;
    constexpr int microsecondsPerSecond = 1000000;
    constexpr uint32_t lowPriorityFrequencyHz = 10000;

    // Current (torque) loop PI gains
    constexpr float currentKp = 15.0f;
    constexpr float currentKi = 2000.0f;
    constexpr float currentKd = 0.0f;

    // Speed loop PID gains
    constexpr float speedKp = 0.1f;
    constexpr float speedKi = 5.0f;
    constexpr float speedKd = 0.0f;

    // Position loop PID gains
    constexpr float positionKp = 10.0f;
    constexpr float positionKi = 0.5f;
    constexpr float positionKd = 0.0f;

    inline hal::Hertz BaseFrequency()
    {
        return hal::Hertz{ static_cast<uint32_t>(microsecondsPerSecond / timeStepMicroseconds) };
    }
}
