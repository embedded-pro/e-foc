#include "can-lite/categories/foc_motor/FocMotorDefinitions.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "source/tool/can_commander/logic/test/CanBusAdapterMock.hpp"
#include "source/tool/can_commander/logic/test/CanCommandClientObserverMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace tool;
    using namespace services;
    using testing::_;
    using testing::Invoke;
    using testing::NiceMock;

    class TestCanCommandClient
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        struct FixtureInit
        {
            FixtureInit(NiceMock<CanBusAdapterMock>& adapter,
                infra::Function<void(hal::Can::Id, const hal::Can::Message&)>& receiveCallback)
            {
                EXPECT_CALL(adapter, ReceiveData(_)).WillOnce([&receiveCallback](const auto& callback)
                    {
                        receiveCallback = callback;
                    });
                ON_CALL(adapter, SendData(_, _, _))
                    .WillByDefault(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                        {
                            cb(true);
                        }));
            }
        };

        NiceMock<CanBusAdapterMock> adapter;
        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> receiveCallback;
        FixtureInit fixtureInit{ adapter, receiveCallback };
        CanCommandClient client{ adapter };
        NiceMock<CanCommandClientObserverMock> observer{ client };
    };

    // ---------- Initial state ----------

    TEST_F(TestCanCommandClient, initially_not_busy)
    {
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClient, default_node_id_is_one)
    {
        EXPECT_EQ(client.NodeId(), 1u);
    }

    // ---------- Node ID ----------

    TEST_F(TestCanCommandClient, set_node_id_is_reflected)
    {
        client.SetNodeId(42);
        EXPECT_EQ(client.NodeId(), 42u);
    }

    // ---------- Timeout ----------

    TEST_F(TestCanCommandClient, handle_timeout_clears_busy_and_notifies)
    {
        EXPECT_CALL(observer, OnBusyChanged(false)).Times(testing::AnyNumber());
        EXPECT_CALL(observer, OnCommandTimeout());

        client.HandleTimeout();

        EXPECT_FALSE(client.IsBusy());
    }

    // ---------- Adapter event relay ----------

    TEST_F(TestCanCommandClient, frame_log_relayed_from_adapter)
    {
        CanFrame data;
        data.push_back(0x42);

        EXPECT_CALL(observer, OnFrameLog(false, 0x12345u, _));

        adapter.NotifyObservers([&data](auto& obs)
            {
                obs.OnFrameLog(false, 0x12345u, data);
            });
    }

    TEST_F(TestCanCommandClient, connection_changed_relayed)
    {
        EXPECT_CALL(observer, OnConnectionChanged(true));

        adapter.NotifyObservers([](auto& obs)
            {
                obs.OnConnectionChanged(true);
            });
    }

    TEST_F(TestCanCommandClient, adapter_error_relayed)
    {
        EXPECT_CALL(observer, OnAdapterError(_));

        adapter.NotifyObservers([](auto& obs)
            {
                obs.OnError("test error");
            });
    }

    // ---------- Command busy management ----------

    TEST_F(TestCanCommandClient, send_start_motor_clears_busy)
    {
        client.SendStartMotor();
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClient, send_speed_setpoint_clears_busy)
    {
        client.SendSetSpeedSetpoint(100.0f);
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClient, send_position_setpoint_clears_busy)
    {
        client.SendSetPositionSetpoint(1.0f);
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClient, send_torque_setpoint_clears_busy)
    {
        client.SendSetTorqueSetpoint(5.0f);
        EXPECT_FALSE(client.IsBusy());
    }

    // ---------- Setpoint clamping ----------

    TEST_F(TestCanCommandClient, send_speed_setpoint_overrange_positive_clears_busy)
    {
        client.SendSetSpeedSetpoint(100000.0f);
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClient, send_speed_setpoint_overrange_negative_clears_busy)
    {
        client.SendSetSpeedSetpoint(-100000.0f);
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClient, send_position_setpoint_overrange_positive_clears_busy)
    {
        client.SendSetPositionSetpoint(1000.0f);
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClient, send_position_setpoint_overrange_negative_clears_busy)
    {
        client.SendSetPositionSetpoint(-1000.0f);
        EXPECT_FALSE(client.IsBusy());
    }
}
