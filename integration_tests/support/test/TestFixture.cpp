#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "e-foc-protocol/contract/FocMotorMessages.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/interactor/interfaces/TargetInteractor.hpp"
#include <chrono>
#include <cstdint>
#include <deque>
#include <gtest/gtest.h>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

namespace
{
    constexpr uint16_t kNode{ 1 };

    hal::Can::Id MakeId(services::CanPriority priority, uint8_t category, uint8_t messageType)
    {
        return hal::Can::Id::Create29BitId(services::MakeCanId(priority, category, messageType, kNode));
    }

    struct Answer
    {
        services::CanAckStatus status;
        std::optional<can::FocMotorCategoryError> error;
    };

    const Answer kSuccess{ services::CanAckStatus::success, std::nullopt };
    const Answer kBusy{ services::CanAckStatus::categoryError, can::FocMotorCategoryError::busy };
    const Answer kInvalidState{ services::CanAckStatus::invalidState, std::nullopt };
    const Answer kCalibrationFailed{ services::CanAckStatus::categoryError, can::FocMotorCategoryError::calibrationFailed };

    class ScriptedTarget
        : public integration::TargetInteractor
    {
    public:
        void Setup() override
        {}

        void Teardown() override
        {}

        void BeforeScenario() override
        {}

        void AfterScenario() override
        {}

        void ClearCanFrames() override
        {
            received.clear();
            pending.clear();
        }

        bool SendCanFrame(hal::Can::Id id, const hal::Can::Message& message, std::chrono::milliseconds) override
        {
            const auto raw = id.Get29BitId();
            if (services::ExtractCanCategory(raw) == can::focMotorCategoryId && services::ExtractCanPriority(raw) == services::CanPriority::command)
            {
                payloads.push_back(message);
                Respond(services::ExtractCanMessageType(raw), message[0]);
            }
            return true;
        }

        bool WaitForCanFrame(hal::Can::Id expectedId, hal::Can::Message& out, std::chrono::milliseconds, std::chrono::steady_clock::time_point, std::chrono::milliseconds& elapsed) override
        {
            elapsed = std::chrono::milliseconds{ 0 };
            if (expectedId == MakeId(services::CanPriority::heartbeat, services::canSystemCategoryId, services::canHeartbeatMessageTypeId))
                return true;

            for (auto frame = pending.begin(); frame != pending.end(); ++frame)
                if (frame->first == expectedId)
                {
                    out = frame->second;
                    pending.erase(frame);
                    return true;
                }

            return false;
        }

        std::size_t ReceivedCanFrameCount() const override
        {
            return received.size();
        }

        std::optional<hal::Can::Message> FindReceivedCanFrame(hal::Can::Id id, std::size_t fromFrame) const override
        {
            std::optional<hal::Can::Message> found;
            for (std::size_t index = fromFrame; index < received.size(); ++index)
                if (received[index].first == id)
                    found = received[index].second;
            return found;
        }

        std::deque<Answer> answers;
        std::size_t commandsSent{ 0 };
        std::vector<hal::Can::Message> payloads;

    private:
        void Respond(uint8_t messageType, uint8_t sequence)
        {
            ++commandsSent;
            const auto answer = answers.size() > 1 ? answers.front() : answers.back();
            if (answers.size() > 1)
                answers.pop_front();

            if (answer.error.has_value())
                Receive(MakeId(services::CanPriority::response, can::focMotorCategoryId, services::canCategoryErrorResponseMessageTypeId),
                    { messageType, static_cast<uint8_t>(*answer.error) });

            Receive(MakeId(services::CanPriority::response, services::canSystemCategoryId, services::canCommandAckMessageTypeId),
                { can::focMotorCategoryId, messageType, static_cast<uint8_t>(answer.status), sequence });
        }

        void Receive(hal::Can::Id id, std::initializer_list<uint8_t> bytes)
        {
            hal::Can::Message message;
            for (const auto byte : bytes)
                message.push_back(byte);
            received.emplace_back(id, message);
            pending.emplace_back(id, message);
        }

        std::vector<std::pair<hal::Can::Id, hal::Can::Message>> received;
        std::deque<std::pair<hal::Can::Id, hal::Can::Message>> pending;
    };

    class CommandWhenFreeTest
        : public testing::Test
    {
    public:
        CommandWhenFreeTest()
        {
            integration::TargetInteractor::SetInstance(target);
        }

        bool Identify()
        {
            integration::Fixture fixture;
            return fixture.SendCommandWhenFree(can::focIdentifyElectricalId, {}, std::chrono::seconds{ 1 }, std::chrono::milliseconds{ 200 });
        }

        bool SetParameter(const hal::Can::Message& payload)
        {
            integration::Fixture fixture;
            return fixture.SendCommandWhenFree(can::focSetParameterId, payload, std::chrono::seconds{ 1 }, std::chrono::milliseconds{ 200 });
        }

        ScriptedTarget target;
    };
}

TEST_F(CommandWhenFreeTest, a_command_answered_busy_is_sent_again_until_it_is_accepted)
{
    target.answers = { kBusy, kBusy, kSuccess };

    EXPECT_TRUE(Identify());
    EXPECT_EQ(target.commandsSent, 3u);
}

TEST_F(CommandWhenFreeTest, a_command_answered_busy_beyond_the_window_fails)
{
    target.answers = { kBusy };

    EXPECT_FALSE(Identify());
    EXPECT_GT(target.commandsSent, 1u);
}

TEST_F(CommandWhenFreeTest, a_command_refused_for_its_state_is_sent_once)
{
    target.answers = { kInvalidState, kSuccess };

    EXPECT_FALSE(Identify());
    EXPECT_EQ(target.commandsSent, 1u);
}

TEST_F(CommandWhenFreeTest, a_command_that_fails_with_another_category_error_is_sent_once)
{
    target.answers = { kCalibrationFailed, kSuccess };

    EXPECT_FALSE(Identify());
    EXPECT_EQ(target.commandsSent, 1u);
}

TEST_F(CommandWhenFreeTest, a_command_answered_busy_is_sent_again_with_the_same_fields)
{
    target.answers = { kBusy, kSuccess };
    const std::vector<uint8_t> fields{ static_cast<uint8_t>(can::FocParameter::telemetryRate), 0x43, 0x48, 0x00, 0x00 };
    hal::Can::Message payload;
    for (const auto byte : fields)
        payload.push_back(byte);

    EXPECT_TRUE(SetParameter(payload));
    ASSERT_EQ(target.payloads.size(), 2u);
    for (const auto& sent : target.payloads)
        EXPECT_EQ(std::vector<uint8_t>(std::next(sent.begin()), sent.end()), fields);
}
