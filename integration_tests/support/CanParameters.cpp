#include "integration_tests/support/CanParameters.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorWireContract.hpp"
#include <bit>
#include <chrono>

namespace integration
{
    namespace
    {
        constexpr auto kSettingTimeout = std::chrono::seconds{ 10 };
        constexpr auto kResponseTimeout = std::chrono::seconds{ 5 };
        constexpr std::size_t kParameterResponseBytes = 6;
        constexpr std::size_t kAlgorithmsResponseBytes = 4;

        hal::Can::Id ResponseId(Fixture& fixture, uint8_t messageType)
        {
            return hal::Can::Id::Create29BitId(services::MakeCanId(services::CanPriority::response, can::focMotorCategoryId, messageType, fixture.ServerNodeId()));
        }
    }

    std::optional<hal::Can::Message> CapturedResponse(Fixture& fixture, uint8_t messageType, Fixture::CaptureMark mark)
    {
        return fixture.WaitForCapturedCanFrame(ResponseId(fixture, messageType), mark, kResponseTimeout);
    }

    std::optional<can::FocParameter> ParameterNamed(const std::string& name)
    {
        for (const auto& descriptor : can::focParameters)
            if (name == descriptor.name)
                return descriptor.id;
        return std::nullopt;
    }

    bool WriteParameter(Fixture& fixture, can::FocParameter parameter, float value)
    {
        const auto bits = std::bit_cast<uint32_t>(value);
        hal::Can::Message payload;
        payload.push_back(static_cast<uint8_t>(parameter));
        for (int shift = 24; shift >= 0; shift -= 8)
            payload.push_back(static_cast<uint8_t>(bits >> shift));
        return fixture.SendCanCommand(can::focMotorCategoryId, can::focSetParameterId, payload, kSettingTimeout);
    }

    std::optional<can::FocParameterValue> ReadParameter(Fixture& fixture, can::FocParameter parameter)
    {
        const auto mark = fixture.MarkCapture();
        hal::Can::Message payload;
        payload.push_back(static_cast<uint8_t>(parameter));
        if (!fixture.SendCanCommand(can::focMotorCategoryId, can::focQueryParameterId, payload, kResponseTimeout))
            return std::nullopt;

        const auto response = CapturedResponse(fixture, can::focParameterResponseId, mark);
        if (!response.has_value() || response->size() != kParameterResponseBytes || (*response)[0] != static_cast<uint8_t>(parameter))
            return std::nullopt;

        uint32_t bits{ 0 };
        for (std::size_t index = 2; index != kParameterResponseBytes; ++index)
            bits = (bits << 8) | (*response)[index];
        return can::FocParameterValue{ std::bit_cast<float>(bits), ((*response)[1] & can::focParameterAutomaticFlag) != 0 };
    }

    std::optional<float> ReadParameterValue(Fixture& fixture, can::FocParameter parameter)
    {
        const auto held = ReadParameter(fixture, parameter);
        if (!held.has_value())
            return std::nullopt;
        return held->value;
    }

    std::optional<hal::Can::Message> ReadAlgorithms(Fixture& fixture)
    {
        const auto mark = fixture.MarkCapture();
        if (!fixture.SendCanCommand(can::focMotorCategoryId, can::focQueryAlgorithmsId, {}, kResponseTimeout))
            return std::nullopt;

        auto response = CapturedResponse(fixture, can::focAlgorithmsResponseId, mark);
        if (!response.has_value() || response->size() != kAlgorithmsResponseBytes)
            return std::nullopt;
        return response;
    }
}
