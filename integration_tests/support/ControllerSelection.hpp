#pragma once

#include "core/can/FocMotorMessages.hpp"
#include "integration_tests/support/Fixture.hpp"
#include <cstdint>
#include <optional>
#include <string>

namespace integration
{
    std::optional<can::FocControlLoop> LoopNamed(const std::string& name);
    std::optional<uint8_t> AlgorithmNamed(can::FocControlLoop loop, const std::string& name);
    bool SelectAlgorithmOverCan(Fixture& fixture, can::FocControlLoop loop, uint8_t algorithm);
}
