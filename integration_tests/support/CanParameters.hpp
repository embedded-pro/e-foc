#pragma once

#include "e-foc-protocol/contract/FocMotorMessages.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/ScenarioSetup.hpp"
#include <optional>
#include <string>

namespace integration
{
    std::optional<hal::Can::Message> CapturedResponse(Fixture& fixture, uint8_t messageType, Fixture::CaptureMark mark);
    std::optional<can::FocParameter> ParameterNamed(const std::string& name);
    bool WriteParameter(Fixture& fixture, can::FocParameter parameter, float value);
    std::optional<can::FocParameterValue> ReadParameter(Fixture& fixture, can::FocParameter parameter);
    std::optional<float> ReadParameterValue(Fixture& fixture, can::FocParameter parameter);
    std::optional<hal::Can::Message> ReadAlgorithms(Fixture& fixture);
    void NoteStoredSettingOnHardware(Fixture& fixture, ScenarioSetup& setup, can::FocParameter parameter);
    bool RestoreStoredSettings(Fixture& fixture, ScenarioSetup& setup);
}
