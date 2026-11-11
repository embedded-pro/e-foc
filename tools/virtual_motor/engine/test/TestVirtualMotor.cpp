#include "can-lite/drivers/hub/VirtualCanHub.hpp"
#include "core/can/FocMotorCanClient.hpp"
#include "generated/Boards.hpp"
#include "infra/util/StaticStorage.hpp"
#include "motor_parameters/PlantPresets.hpp"
#include "targets/sync_foc_sensored/main/instantiations/Logic.hpp"
#include "tools/virtual_motor/platform/VirtualPlatform.hpp"
#include "gtest/gtest.h"
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#if defined(_WIN32)
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

namespace
{
    class ServerTracker
        : public services::CanProtocolClientObserver
    {
    public:
        using services::CanProtocolClientObserver::CanProtocolClientObserver;

        void OnServerOnline(uint16_t nodeId) override
        {
            online = nodeId;
        }

        void OnServerOffline(uint16_t) override
        {
            online.reset();
        }

        void OnCommandAckTimeout(uint16_t, uint8_t, uint8_t) override
        {}

        std::optional<uint16_t> online;
    };

    class Responses
        : public can::FocMotorCategoryClientObserver
    {
    public:
        using can::FocMotorCategoryClientObserver::FocMotorCategoryClientObserver;

        void OnSelectControlModeResponse(uint16_t, can::FocMotorMode activeMode) override
        {
            mode = activeMode;
        }

        void OnCategoryError(uint16_t, uint8_t, can::FocMotorCategoryError) override
        {}

        void OnTelemetryStatus(uint16_t, can::FocMotorState newState, can::FocFaultCode) override
        {
            state = newState;
        }

        void OnTelemetryMotion(uint16_t, foc::RadiansPerSecond newSpeed, foc::Radians) override
        {
            speed = newSpeed.Value();
        }

        void OnTelemetryCurrents(uint16_t, uint8_t, foc::Ampere, foc::Ampere, foc::Radians) override
        {}

        void OnContractVersionResponse(uint16_t, uint8_t, uint8_t) override
        {}

        void OnAlgorithmsResponse(uint16_t, const can::FocAlgorithms&) override
        {}

        void OnParameterResponse(uint16_t, can::FocParameter parameter, can::FocParameterValue value) override
        {
            if (parameter == can::FocParameter::boardKind)
                boardKind = value.value;
        }

        void OnFaultRecordResponse(uint16_t, uint8_t, can::FocFaultCode, can::FocMotorMode, uint32_t) override
        {}

        void OnFaultConditionsResponse(uint16_t, uint8_t, int16_t, foc::Volts, foc::Ampere) override
        {}

        std::optional<can::FocMotorState> state;
        std::optional<can::FocMotorMode> mode;
        std::optional<float> boardKind;
        std::optional<float> speed;
    };

    uint16_t FreePort()
    {
        const auto probe = services::loopback::Listen(0);
        const auto port = services::loopback::LocalPort(probe).value_or(0);
        services::loopback::Close(probe);
        return port;
    }

    std::string MemoryFile()
    {
        return (std::filesystem::temp_directory_path() / ("e-foc-virtual-motor-test-" + std::to_string(getpid()) + ".nvm")).string();
    }

    infra::StaticStorage<virtual_motor::VirtualPlatform> platform;
    infra::StaticStorage<services::VirtualCanHub> hub;
    infra::StaticStorage<application::Logic> logic;
    infra::StaticStorage<virtual_motor::VirtualCanPort> clientPort;
    infra::StaticStorage<can::FocMotorCanClient> client;

    void RunFor(uint32_t milliseconds)
    {
        for (uint32_t millisecond = 0; millisecond != milliseconds; ++millisecond)
        {
            platform->AdvanceOneMillisecond();
            clientPort->Poll();
        }
    }

    bool RunUntil(const std::function<bool()>& done, uint32_t milliseconds)
    {
        for (uint32_t millisecond = 0; millisecond != milliseconds && !done(); ++millisecond)
        {
            platform->AdvanceOneMillisecond();
            clientPort->Poll();
        }
        return done();
    }
}

TEST(TestVirtualMotor, a_client_on_the_virtual_bus_spins_a_factory_fresh_motor_that_goes_silent_while_paused)
{
    const auto port = FreePort();
    const auto memory = MemoryFile();
    std::filesystem::remove(memory);

    platform.Construct(virtual_motor::BoardOptions{ "test", 0, sil::NominalPlant(), memory, port, false, {}, "nominal" });
    hub.Construct();
    ASSERT_TRUE(hub->Listen(port));
    platform->EachMillisecond([]()
        {
            hub->Poll();
        });
    logic.Construct(*platform);

    clientPort.Construct(port);
    client.Construct(*clientPort);
    ServerTracker servers{ client->ProtocolClient() };
    Responses responses{ client->CategoryClient() };
    ASSERT_TRUE(clientPort->Connect("test", 1000000));

    ASSERT_TRUE(RunUntil([&servers]()
        {
            return servers.online.has_value();
        },
        5000));

    client->CategoryClient().SendQueryParameter(*servers.online, can::FocParameter::boardKind);
    ASSERT_TRUE(RunUntil([&responses]()
        {
            return responses.boardKind.has_value();
        },
        1000));
    EXPECT_EQ(*responses.boardKind, static_cast<float>(e_foc::boards::virtualBoard.kind));

    client->CategoryClient().SendRequestTelemetry(*servers.online);
    ASSERT_TRUE(RunUntil([&responses]()
        {
            return responses.state.has_value();
        },
        1000));
    EXPECT_EQ(*responses.state, can::FocMotorState::idle);

    const auto node = *servers.online;
    const auto statusIs = [&responses, node](can::FocMotorState wanted)
    {
        responses.state.reset();
        client->CategoryClient().SendRequestTelemetry(node);
        RunFor(200);
        return responses.state == wanted;
    };

    ASSERT_TRUE(client->CategoryClient().SendSelectControlMode(node, can::FocMotorMode::speed));
    ASSERT_TRUE(RunUntil([&responses]()
        {
            return responses.mode == can::FocMotorMode::speed;
        },
        1000));

    ASSERT_TRUE(client->CategoryClient().SendMaintenanceCommand(node, can::FocMaintenanceCommand::calibrate));
    ASSERT_TRUE(RunUntil([&statusIs]()
        {
            return statusIs(can::FocMotorState::ready);
        },
        120000))
        << "the virtual motor never finished calibrating";

    ASSERT_TRUE(client->CategoryClient().SendAlign(node));
    RunFor(3000);
    ASSERT_TRUE(client->CategoryClient().SendStart(node));
    ASSERT_TRUE(RunUntil([&statusIs]()
        {
            return statusIs(can::FocMotorState::running);
        },
        15000))
        << "the virtual motor never started";

    ASSERT_TRUE(client->CategoryClient().SendSetpoint(node, can::FocSetpoint{ can::FocSetpointKind::speed, 100.0f }));
    RunFor(3000);
    responses.speed.reset();
    client->CategoryClient().SendRequestTelemetry(node);
    ASSERT_TRUE(RunUntil([&responses]()
        {
            return responses.speed.has_value();
        },
        1000));
    EXPECT_NEAR(*responses.speed, 100.0f, 5.0f);

    platform->Runner().Pause();
    for (int millisecond = 0; millisecond != 10000 && servers.online.has_value(); ++millisecond)
    {
        platform->Runner().Step(virtual_motor::port::StepUnit::millisecond, 1);
        clientPort->Poll();
    }
    EXPECT_FALSE(servers.online.has_value()) << "a paused motor kept talking on the bus";

    platform->Runner().Resume();
    EXPECT_TRUE(RunUntil([&servers]()
        {
            return servers.online.has_value();
        },
        5000))
        << "the resumed motor did not come back online";

    std::filesystem::remove(memory);
}
