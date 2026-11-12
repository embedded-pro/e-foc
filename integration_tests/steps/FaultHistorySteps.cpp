#include "cucumber_cpp/Steps.hpp"
#include "e-foc-protocol/contract/FocMotorMessages.hpp"
#include "integration_tests/support/CanParameters.hpp"
#include "integration_tests/support/Fixture.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <thread>

using namespace integration;

namespace
{
    constexpr std::size_t kFaultRecordBytes = 7;
    constexpr std::size_t kFaultConditionsBytes = 7;
    constexpr auto kQueryTimeout = std::chrono::seconds{ 5 };

    struct ReadFaultRecord
    {
        can::FocFaultCode code;
        can::FocMotorMode mode;
        uint32_t uptimeMs;
        int16_t powerUp;
        float busVoltage;
        float peakCurrent;
    };

    int16_t Int16At(const hal::Can::Message& frame, std::size_t index)
    {
        return static_cast<int16_t>((frame[index] << 8) | frame[index + 1]);
    }

    std::optional<ReadFaultRecord> QueryFaultRecord(Fixture& fixture, uint8_t age)
    {
        const auto mark = fixture.MarkCapture();
        hal::Can::Message payload;
        payload.push_back(age);
        if (!fixture.SendCanCommand(can::focMotorCategoryId, can::focQueryFaultHistoryId, payload, kQueryTimeout))
            return std::nullopt;

        const auto record = CapturedResponse(fixture, can::focFaultRecordResponseId, mark);
        const auto conditions = CapturedResponse(fixture, can::focFaultConditionsResponseId, mark);
        if (!record.has_value() || record->size() != kFaultRecordBytes || (*record)[0] != age)
            return std::nullopt;
        if (!conditions.has_value() || conditions->size() != kFaultConditionsBytes || (*conditions)[0] != age)
            return std::nullopt;

        uint32_t uptime{ 0 };
        for (std::size_t index = 3; index != kFaultRecordBytes; ++index)
            uptime = (uptime << 8) | (*record)[index];

        return ReadFaultRecord{ static_cast<can::FocFaultCode>((*record)[1]), static_cast<can::FocMotorMode>((*record)[2]), uptime,
            Int16At(*conditions, 1), Int16At(*conditions, 3) / static_cast<float>(can::focRecordedVoltageScale),
            Int16At(*conditions, 5) / static_cast<float>(can::focRecordedCurrentScale) };
    }

    std::optional<can::FocFaultCode> RecordedFaultNamed(const std::string& name)
    {
        if (name == "overvoltage")
            return can::FocFaultCode::overVoltage;
        if (name == "undervoltage")
            return can::FocFaultCode::underVoltage;
        return std::nullopt;
    }

    std::optional<can::FocMotorMode> ModeNamed(const std::string& name)
    {
        if (name == "torque")
            return can::FocMotorMode::torque;
        if (name == "speed")
            return can::FocMotorMode::speed;
        if (name == "position")
            return can::FocMotorMode::position;
        return std::nullopt;
    }
}

WHEN(R"(the drive is left for {int} ms)", (int milliseconds))
{
    std::this_thread::sleep_for(std::chrono::milliseconds{ milliseconds });
}

WHEN(R"(the fault history is cleared over CAN)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focClearFaultHistoryId, {}, kQueryTimeout))
        << "The target did not acknowledge the cleared fault history";
}

THEN(R"(fault record {int} shall be a(n) {word} fault in {word} mode from power-up {int})", (int age, std::string fault, std::string mode, int powerUp))
{
    auto& fixture = context.Get<Fixture>();
    const auto expectedCode = RecordedFaultNamed(fault);
    const auto expectedMode = ModeNamed(mode);
    ASSERT_TRUE(expectedCode.has_value()) << "Unknown fault: " << fault;
    ASSERT_TRUE(expectedMode.has_value()) << "Unknown mode: " << mode;

    const auto record = QueryFaultRecord(fixture, static_cast<uint8_t>(age));
    ASSERT_TRUE(record.has_value()) << "The target did not answer the fault history query for age " << age;
    EXPECT_EQ(static_cast<int>(*expectedCode), static_cast<int>(record->code)) << "fault code of record " << age;
    EXPECT_EQ(static_cast<int>(*expectedMode), static_cast<int>(record->mode)) << "control mode of record " << age;
    EXPECT_EQ(powerUp, record->powerUp) << "power-up of record " << age;
}

THEN(R"(fault record {int} shall be a(n) {word} fault in {word} mode)", (int age, std::string fault, std::string mode))
{
    auto& fixture = context.Get<Fixture>();
    const auto expectedCode = RecordedFaultNamed(fault);
    const auto expectedMode = ModeNamed(mode);
    ASSERT_TRUE(expectedCode.has_value()) << "Unknown fault: " << fault;
    ASSERT_TRUE(expectedMode.has_value()) << "Unknown mode: " << mode;

    const auto record = QueryFaultRecord(fixture, static_cast<uint8_t>(age));
    ASSERT_TRUE(record.has_value()) << "The target did not answer the fault history query for age " << age;
    EXPECT_EQ(static_cast<int>(*expectedCode), static_cast<int>(record->code)) << "fault code of record " << age;
    EXPECT_EQ(static_cast<int>(*expectedMode), static_cast<int>(record->mode)) << "control mode of record " << age;
}

THEN(R"(fault record {int} shall come from the power-up before that of record {int})", (int older, int newer))
{
    auto& fixture = context.Get<Fixture>();
    const auto olderRecord = QueryFaultRecord(fixture, static_cast<uint8_t>(older));
    const auto newerRecord = QueryFaultRecord(fixture, static_cast<uint8_t>(newer));
    ASSERT_TRUE(olderRecord.has_value() && newerRecord.has_value()) << "The target did not answer the fault history queries";
    std::printf("[METRIC] hil fault_record_power_ups older=%d newer=%d\n", olderRecord->powerUp, newerRecord->powerUp);
    EXPECT_EQ(newerRecord->powerUp - 1, olderRecord->powerUp);
}

THEN(R"(fault record {int} shall carry a bus voltage within {float} V of {float} V)", (int age, float tolerance, float expected))
{
    auto& fixture = context.Get<Fixture>();
    const auto record = QueryFaultRecord(fixture, static_cast<uint8_t>(age));
    ASSERT_TRUE(record.has_value()) << "The target did not answer the fault history query for age " << age;
    std::printf("[METRIC] sil fault_record_%d_bus_voltage value=%.2f peak_current=%.2f uptime_ms=%u\n", age, static_cast<double>(record->busVoltage),
        static_cast<double>(record->peakCurrent), static_cast<unsigned>(record->uptimeMs));
    EXPECT_NEAR(expected, record->busVoltage, tolerance);
}

THEN(R"(the fault history shall hold no record {int})", (int age))
{
    auto& fixture = context.Get<Fixture>();
    const auto record = QueryFaultRecord(fixture, static_cast<uint8_t>(age));
    ASSERT_TRUE(record.has_value()) << "The target did not answer the fault history query for age " << age;
    EXPECT_EQ(static_cast<int>(can::FocFaultCode::none), static_cast<int>(record->code)) << "record " << age << " is not empty";
    EXPECT_EQ(0u, record->uptimeMs);
    EXPECT_EQ(0, record->powerUp);
}
