#include "foc/interfaces/Driver.hpp"
#include "hal/interfaces/test_doubles/SerialCommunicationMock.hpp"
#include "infra/event/test_helper/EventDispatcherWithWeakPtrFixture.hpp"
#include "infra/util/test_helper/MockHelpers.hpp"
#include "infra/util/test_helper/ProxyCreatorMock.hpp"
#include "platform_abstraction/AdcPhaseCurrentMeasurement.hpp"
#include "services/tracer/Tracer.hpp"
#include "targets/hardware_test/components/Terminal.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    class StreamWriterMock
        : public infra::StreamWriter
    {
    public:
        MOCK_METHOD(void, Insert, (infra::ConstByteRange range, infra::StreamErrorPolicy& errorPolicy), (override));
        MOCK_METHOD(std::size_t, Available, (), (const override));
        MOCK_METHOD(std::size_t, ConstructSaveMarker, (), (const override));
        MOCK_METHOD(std::size_t, GetProcessedBytesSince, (std::size_t marker), (const override));
        MOCK_METHOD(infra::ByteRange, SaveState, (std::size_t marker), (override));
        MOCK_METHOD(void, RestoreState, (infra::ByteRange range), (override));
        MOCK_METHOD(infra::ByteRange, Overwrite, (std::size_t marker), (override));
    };

    class HardwareFactoryMock
        : public application::HardwareFactory
    {
    public:
        MOCK_METHOD(void, Run, (), (override));
        MOCK_METHOD(services::Tracer&, Tracer, (), (override));
        MOCK_METHOD(services::TerminalWithCommands&, Terminal, (), (override));
        MOCK_METHOD(infra::MemoryRange<hal::GpioPin>, Leds, (), (override));
        MOCK_METHOD(hal::PerformanceTracker&, PerformanceTimer, (), (override));
        MOCK_METHOD(hal::Hertz, SystemClock, (), (const, override));
        MOCK_METHOD(foc::Volts, PowerSupplyVoltage, (), (override));
        MOCK_METHOD(foc::Ampere, MaxCurrentSupported, (), (override));
        MOCK_METHOD((foc::LowPriorityInterrupt&), LowPriorityInterrupt, (), (override));
        MOCK_METHOD((infra::CreatorBase<hal::SynchronousThreeChannelsPwm, void(std::chrono::nanoseconds, hal::Hertz)>&), SynchronousThreeChannelsPwmCreator, (), (override));
        MOCK_METHOD((infra::CreatorBase<application::AdcPhaseCurrentMeasurement, void(SampleAndHold)>&), AdcMultiChannelCreator, (), (override));
        MOCK_METHOD((infra::CreatorBase<application::QuadratureEncoderDecorator, void()>&), SynchronousQuadratureEncoderCreator, (), (override));
        MOCK_METHOD((infra::CreatorBase<application::CanBusAdapter, void(uint32_t, bool)>&), CanBusCreator, (), (override));
    };

    class PwmMock
        : public hal::SynchronousThreeChannelsPwm
    {
    public:
        MOCK_METHOD(void, SetBaseFrequency, (hal::Hertz baseFrequency), (override));
        MOCK_METHOD(void, Stop, (), (override));
        MOCK_METHOD(void, Start, (hal::Percent dutyCycle1, hal::Percent dutyCycle2, hal::Percent dutyCycle3), (override));
    };

    class AdcMock
        : public hal::AdcMultiChannel
    {
    public:
        MOCK_METHOD(void, Measure, (const infra::Function<void(Samples)>& onDone), (override));
        MOCK_METHOD(void, Stop, (), (override));
    };

    class EncoderMock
        : public hal::SynchronousQuadratureEncoder
    {
    public:
        MOCK_METHOD(uint32_t, Position, (), (override));
        MOCK_METHOD(uint32_t, Resolution, (), (override));
        MOCK_METHOD(MotionDirection, Direction, (), (override));
        MOCK_METHOD(uint32_t, Speed, (), (override));
    };

    class AdcPhaseCurrentMeasurementMock
        : public application::AdcPhaseCurrentMeasurement
    {
    public:
        MOCK_METHOD(void, Measure, ((const infra::Function<void(foc::Ampere phaseA, foc::Ampere phaseB, foc::Ampere phaseC)>& onDone)), (override));
        MOCK_METHOD(void, Stop, (), (override));
    };

    class QuadratureEncoderDecoratorMock
        : public application::QuadratureEncoderDecorator
    {
    public:
        MOCK_METHOD(foc::Radians, Read, (), (override));
    };

    class CanBusAdapterMock
        : public application::CanBusAdapter
    {
    public:
        MOCK_METHOD(void, SendData, (Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion), (override));
        MOCK_METHOD(void, ReceiveData, (const infra::Function<void(Id id, const Message& data)>& receivedAction), (override));
        MOCK_METHOD(void, SetOnError, (const infra::Function<void(CanError)>& handler), (override));
    };

    class PerformanceTrackerMock
        : public hal::PerformanceTracker
    {
    public:
        MOCK_METHOD(void, Start, (), (override));
        MOCK_METHOD(uint32_t, ElapsedCycles, (), (override));
    };

    class SimpleLowPriorityInterrupt
        : public foc::LowPriorityInterrupt
    {
    public:
        void Trigger() override
        {
            if (handler)
                handler();
        }

        void Register(const infra::Function<void()>& handler) override
        {
            this->handler = handler;
        }

    private:
        infra::Function<void()> handler;
    };

    class TestHardwareTerminal
        : public testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        TestHardwareTerminal()
        {
            EXPECT_CALL(hardwareFactoryMock, Terminal()).WillRepeatedly(testing::ReturnRef(terminalWithCommands));
            EXPECT_CALL(hardwareFactoryMock, Tracer()).WillRepeatedly(testing::ReturnRef(tracer));
            EXPECT_CALL(hardwareFactoryMock, SynchronousThreeChannelsPwmCreator()).WillRepeatedly(testing::ReturnRef(pwmCreator));
            EXPECT_CALL(hardwareFactoryMock, AdcMultiChannelCreator()).WillRepeatedly(testing::ReturnRef(adcCreator));
            EXPECT_CALL(hardwareFactoryMock, SynchronousQuadratureEncoderCreator()).WillRepeatedly(testing::ReturnRef(encoderCreator));
            EXPECT_CALL(hardwareFactoryMock, CanBusCreator()).WillRepeatedly(testing::ReturnRef(canCreator));
            EXPECT_CALL(hardwareFactoryMock, PerformanceTimer()).WillRepeatedly(testing::ReturnRef(performanceTrackerMock));
            EXPECT_CALL(hardwareFactoryMock, PowerSupplyVoltage()).WillRepeatedly(testing::Return(foc::Volts{ 24.0f }));
            EXPECT_CALL(hardwareFactoryMock, MaxCurrentSupported()).WillRepeatedly(testing::Return(foc::Ampere{ 5.0f }));
            EXPECT_CALL(hardwareFactoryMock, SystemClock()).WillRepeatedly(testing::Return(hal::Hertz{ 10000 }));
            EXPECT_CALL(hardwareFactoryMock, LowPriorityInterrupt()).WillRepeatedly(testing::ReturnRef(simpleLowPriorityInterrupt));

            EXPECT_CALL(encoderCreator, Constructed());
            EXPECT_CALL(pwmCreator, Constructed(std::chrono::nanoseconds{ 500 }, hal::Hertz{ 10000 }));
            EXPECT_CALL(adcCreator, Constructed(application::HardwareFactory::SampleAndHold::shortest));

            EXPECT_CALL(adcDecoratorMock, Measure(testing::_)).WillRepeatedly(testing::SaveArg<0>(&onAdcMeasurementDone));

            terminalInteractor.emplace(terminal, hardwareFactoryMock);
        }

        ~TestHardwareTerminal()
        {
            EXPECT_CALL(encoderCreator, Destructed()).Times(1);
            EXPECT_CALL(pwmCreator, Destructed()).Times(testing::AtLeast(1));
            EXPECT_CALL(adcCreator, Destructed()).Times(testing::AtLeast(1));
        }

        testing::StrictMock<HardwareFactoryMock> hardwareFactoryMock;
        SimpleLowPriorityInterrupt simpleLowPriorityInterrupt;
        testing::StrictMock<StreamWriterMock> streamWriterMock;
        infra::TextOutputStream::WithErrorPolicy stream{ streamWriterMock };
        services::TracerToStream tracer{ stream };
        testing::StrictMock<hal::SerialCommunicationMock> communication;
        infra::Execute execute{ [this]()
            {
                EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
            } };
        services::TerminalWithCommandsImpl::WithMaxQueueAndMaxHistory<128, 5> terminalWithCommands{ communication, tracer };
        services::TerminalWithStorage::WithMaxSize<14> terminal{ terminalWithCommands, tracer };

        testing::StrictMock<PwmMock> pwmMock;
        testing::StrictMock<AdcMock> adcMock;
        testing::StrictMock<EncoderMock> encoderMock;
        testing::NiceMock<PerformanceTrackerMock> performanceTrackerMock;
        testing::StrictMock<AdcPhaseCurrentMeasurementMock> adcDecoratorMock;
        testing::StrictMock<QuadratureEncoderDecoratorMock> encoderDecoratorMock;

        infra::CreatorMock<hal::SynchronousThreeChannelsPwm, void(std::chrono::nanoseconds, hal::Hertz)> pwmCreator{ pwmMock };
        infra::CreatorMock<application::AdcPhaseCurrentMeasurement, void(application::HardwareFactory::SampleAndHold)> adcCreator{ adcDecoratorMock };
        infra::CreatorMock<application::QuadratureEncoderDecorator, void()> encoderCreator{ encoderDecoratorMock };
        testing::NiceMock<CanBusAdapterMock> canAdapterMock;
        infra::CreatorMock<application::CanBusAdapter, void(uint32_t, bool)> canCreator{ canAdapterMock };

        std::optional<application::TerminalInteractor> terminalInteractor;
        infra::Function<void(foc::Ampere, foc::Ampere, foc::Ampere)> onAdcMeasurementDone;

        void InvokeCommand(std::string command, const std::function<void()>& onCommandReceived)
        {
            ::testing::InSequence _;

            for (const auto& data : command)
                EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>{ static_cast<uint8_t>(data) }), testing::_));

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>{ { '\r', '\n' } }), testing::_));
            onCommandReceived();
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>{ { '>', ' ' } }), testing::_));

            communication.dataReceived(infra::MakeStringByteRange(command + "\r"));
        }

        void Payload(std::string header, std::string mean, std::string deviationStandard, std::string footer)
        {
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>{ { '\r', '\n' } }), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));

            for (char c : mean)
                EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>{ static_cast<uint8_t>(c) }), testing::_));

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>{ ',' }), testing::_));

            for (char c : deviationStandard)
                EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>{ static_cast<uint8_t>(c) }), testing::_));

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(footer.begin(), footer.end())), testing::_));
        }
    };
}

TEST_F(TestHardwareTerminal, stop_command)
{
    InvokeCommand("stop", [this]()
        {
            EXPECT_CALL(pwmMock, Stop());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, stop_alias)
{
    InvokeCommand("stp", [this]()
        {
            EXPECT_CALL(pwmMock, Stop());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, duty_command)
{
    InvokeCommand("duty 10 20 30", [this]()
        {
            EXPECT_CALL(pwmMock, Start(hal::Percent{ 10 }, hal::Percent{ 20 }, hal::Percent{ 30 }));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, duty_alias)
{
    InvokeCommand("d 15 25 35", [this]()
        {
            EXPECT_CALL(pwmMock, Start(hal::Percent{ 15 }, hal::Percent{ 25 }, hal::Percent{ 35 }));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, duty_invalid_argument_count)
{
    InvokeCommand("duty 10 20", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid number of arguments" };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, pwm_command)
{
    InvokeCommand("pwm 500 10000", [this]()
        {
            EXPECT_CALL(pwmCreator, Destructed()).Times(1);
            EXPECT_CALL(pwmCreator, Constructed(std::chrono::nanoseconds{ 500 }, hal::Hertz{ 10000 })).Times(1);
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, pwm_alias)
{
    InvokeCommand("p 750 15000", [this]()
        {
            EXPECT_CALL(pwmCreator, Destructed()).Times(1);
            EXPECT_CALL(pwmCreator, Constructed(std::chrono::nanoseconds{ 750 }, hal::Hertz{ 15000 })).Times(1);
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, adc_command)
{
    InvokeCommand("adc medium", [this]()
        {
            EXPECT_CALL(adcCreator, Constructed(application::HardwareFactory::SampleAndHold::medium)).Times(1);
            EXPECT_CALL(adcDecoratorMock, Measure(testing::_)).Times(1);
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, adc_alias)
{
    InvokeCommand("a shortest", [this]()
        {
            EXPECT_CALL(adcCreator, Constructed(application::HardwareFactory::SampleAndHold::shortest)).Times(1);
            EXPECT_CALL(adcDecoratorMock, Measure(testing::_)).Times(1);
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, adc_invalid_value)
{
    InvokeCommand("adc invalid", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid value. It should be one of: shortest, shorter, medium, longer, longest." };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, pid_command)
{
    InvokeCommand("pid 1.0 0.5 0.1 2.0 1.0 0.2", [this]()
        {
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, pid_alias)
{
    InvokeCommand("c 1.5 0.75 0.15 2.5 1.5 0.25", [this]()
        {
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, pid_invalid_argument_count)
{
    InvokeCommand("pid 1.0 0.5 0.1", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid number of arguments" };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, pid_invalid_speed_kp)
{
    InvokeCommand("pid invalid 0.5 0.1 2.0 1.0 0.2", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid value for Speed Kp" };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, pid_invalid_dq_ki)
{
    InvokeCommand("pid 1.0 0.5 0.1 2.0 invalid 0.2", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid value for DQ-axis Ki" };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, foc_command)
{
    InvokeCommand("foc 45.0 1.5 2.0 2.5", [this]()
        {
            EXPECT_CALL(performanceTrackerMock, Start());
            EXPECT_CALL(performanceTrackerMock, ElapsedCycles()).WillOnce(testing::Return(1000));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, foc_alias)
{
    InvokeCommand("f 90.0 2.0 3.0 4.0", [this]()
        {
            EXPECT_CALL(performanceTrackerMock, Start());
            EXPECT_CALL(performanceTrackerMock, ElapsedCycles()).WillOnce(testing::Return(1500));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, foc_invalid_argument_count)
{
    InvokeCommand("foc 45.0 1.5", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid number of arguments" };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, foc_invalid_angle)
{
    InvokeCommand("foc 400.0 1.5 2.0 2.5", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid value for angle. It should be a float between -360 and 360." };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, foc_invalid_phase_a_current)
{
    InvokeCommand("foc 45.0 invalid 2.0 2.5", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid value for phase A current. It should be a float between -1000 and 1000." };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, foc_invalid_phase_c_current)
{
    InvokeCommand("foc 45.0 1.5 2.0 1500.0", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid value for phase C current. It should be a float between -1000 and 1000." };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, foc_with_negative_angle)
{
    InvokeCommand("foc -90.0 1.5 2.0 2.5", [this]()
        {
            EXPECT_CALL(performanceTrackerMock, Start());
            EXPECT_CALL(performanceTrackerMock, ElapsedCycles()).WillOnce(testing::Return(1200));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, foc_with_negative_currents)
{
    InvokeCommand("foc 45.0 -1.5 -2.0 -2.5", [this]()
        {
            EXPECT_CALL(performanceTrackerMock, Start());
            EXPECT_CALL(performanceTrackerMock, ElapsedCycles()).WillOnce(testing::Return(1100));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, foc_simulation_output_format)
{
    InvokeCommand("foc 0.0 0.0 0.0 0.0", [this]()
        {
            ::testing::InSequence _;

            EXPECT_CALL(performanceTrackerMock, Start());
            EXPECT_CALL(performanceTrackerMock, ElapsedCycles()).WillOnce(testing::Return(2000));

            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, foc_with_maximum_angle)
{
    InvokeCommand("foc 360.0 1.0 2.0 3.0", [this]()
        {
            EXPECT_CALL(performanceTrackerMock, Start());
            EXPECT_CALL(performanceTrackerMock, ElapsedCycles()).WillOnce(testing::Return(1300));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, foc_with_minimum_angle)
{
    InvokeCommand("foc -360.0 1.0 2.0 3.0", [this]()
        {
            EXPECT_CALL(performanceTrackerMock, Start());
            EXPECT_CALL(performanceTrackerMock, ElapsedCycles()).WillOnce(testing::Return(1250));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, foc_with_maximum_currents)
{
    InvokeCommand("foc 45.0 1000.0 1000.0 1000.0", [this]()
        {
            EXPECT_CALL(performanceTrackerMock, Start());
            EXPECT_CALL(performanceTrackerMock, ElapsedCycles()).WillOnce(testing::Return(1400));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, foc_with_minimum_currents)
{
    InvokeCommand("foc 45.0 -1000.0 -1000.0 -1000.0", [this]()
        {
            EXPECT_CALL(performanceTrackerMock, Start());
            EXPECT_CALL(performanceTrackerMock, ElapsedCycles()).WillOnce(testing::Return(1350));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, foc_multiple_simulations)
{
    InvokeCommand("foc 30.0 1.0 2.0 3.0", [this]()
        {
            EXPECT_CALL(performanceTrackerMock, Start());
            EXPECT_CALL(performanceTrackerMock, ElapsedCycles()).WillOnce(testing::Return(1050));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();

    InvokeCommand("foc 60.0 2.0 3.0 4.0", [this]()
        {
            EXPECT_CALL(performanceTrackerMock, Start());
            EXPECT_CALL(performanceTrackerMock, ElapsedCycles()).WillOnce(testing::Return(1150));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, foc_with_fractional_values)
{
    InvokeCommand("foc 45.5 1.234 2.567 3.891", [this]()
        {
            EXPECT_CALL(performanceTrackerMock, Start());
            EXPECT_CALL(performanceTrackerMock, ElapsedCycles()).WillOnce(testing::Return(1075));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, motor_command)
{
    InvokeCommand("motor 14", [this]()
        {
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, motor_alias)
{
    InvokeCommand("m 8", [this]()
        {
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, motor_invalid_argument_count)
{
    InvokeCommand("motor 14 8", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid number of arguments" };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, motor_invalid_poles_too_low)
{
    InvokeCommand("motor 1", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid value for poles. It should be an integer between 2 and 16." };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, motor_invalid_poles_too_high)
{
    InvokeCommand("motor 18", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid value for poles. It should be an integer between 2 and 16." };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, motor_invalid_poles_not_a_number)
{
    InvokeCommand("motor invalid", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid value for poles. It should be an integer between 2 and 16." };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, motor_minimum_valid_poles)
{
    InvokeCommand("motor 2", [this]()
        {
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, motor_maximum_valid_poles)
{
    InvokeCommand("motor 16", [this]()
        {
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, pwm_invalid_argument_count_too_few)
{
    InvokeCommand("pwm 500", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid number of arguments" };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, pwm_invalid_argument_count_too_many)
{
    InvokeCommand("pwm 500 10000 extra", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid number of arguments" };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, pwm_invalid_dead_time_too_low)
{
    InvokeCommand("pwm 100 10000", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid value. It should be a float between 500 and 2000." };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, pwm_invalid_dead_time_too_high)
{
    InvokeCommand("pwm 3000 10000", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid value. It should be a float between 500 and 2000." };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, pwm_invalid_frequency_too_low)
{
    InvokeCommand("pwm 500 5000", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid value. It should be a float between 10000 and 20000." };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, pwm_invalid_frequency_too_high)
{
    InvokeCommand("pwm 500 25000", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid value. It should be a float between 10000 and 20000." };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, adc_shorter)
{
    InvokeCommand("adc shorter", [this]()
        {
            EXPECT_CALL(adcCreator, Constructed(application::HardwareFactory::SampleAndHold::shorter)).Times(1);
            EXPECT_CALL(adcDecoratorMock, Measure(testing::_)).Times(1);
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, adc_longer)
{
    InvokeCommand("adc longer", [this]()
        {
            EXPECT_CALL(adcCreator, Constructed(application::HardwareFactory::SampleAndHold::longer)).Times(1);
            EXPECT_CALL(adcDecoratorMock, Measure(testing::_)).Times(1);
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, adc_longest)
{
    InvokeCommand("adc longest", [this]()
        {
            EXPECT_CALL(adcCreator, Constructed(application::HardwareFactory::SampleAndHold::longest)).Times(1);
            EXPECT_CALL(adcDecoratorMock, Measure(testing::_)).Times(1);
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, adc_invalid_argument_count)
{
    InvokeCommand("adc medium extra", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid number of arguments" };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, pid_invalid_speed_ki)
{
    InvokeCommand("pid 1.0 invalid 0.1 2.0 1.0 0.2", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid value for Speed Ki" };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, pid_invalid_speed_kd)
{
    InvokeCommand("pid 1.0 0.5 invalid 2.0 1.0 0.2", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid value for Speed Kd" };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, pid_invalid_dq_kp)
{
    InvokeCommand("pid 1.0 0.5 0.1 invalid 1.0 0.2", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid value for DQ-axis Kp" };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, pid_invalid_dq_kd)
{
    InvokeCommand("pid 1.0 0.5 0.1 2.0 1.0 invalid", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid value for DQ-axis Kd" };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, duty_invalid_phase_a)
{
    InvokeCommand("duty 0 20 30", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid value for phase A. It should be a float between 1 and 99." };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, duty_invalid_phase_b)
{
    InvokeCommand("duty 10 100 30", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid value for phase B. It should be a float between 1 and 99." };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, duty_invalid_phase_c)
{
    InvokeCommand("duty 10 20 invalid", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid value for phase C. It should be a float between 1 and 99." };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, foc_invalid_phase_b_current)
{
    InvokeCommand("foc 45.0 1.5 invalid 2.5", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid value for phase B current. It should be a float between -1000 and 1000." };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, foc_invalid_angle_non_numeric)
{
    InvokeCommand("foc invalid 1.5 2.0 2.5", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid value for angle. It should be a float between -360 and 360." };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, pwm_boundary_values_valid)
{
    InvokeCommand("pwm 2000 20000", [this]()
        {
            EXPECT_CALL(pwmCreator, Destructed()).Times(1);
            EXPECT_CALL(pwmCreator, Constructed(std::chrono::nanoseconds{ 2000 }, hal::Hertz{ 20000 })).Times(1);
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, duty_boundary_values_valid)
{
    InvokeCommand("duty 1 50 99", [this]()
        {
            EXPECT_CALL(pwmMock, Start(hal::Percent{ 1 }, hal::Percent{ 50 }, hal::Percent{ 99 }));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, adc_samples_stored_when_buffer_not_full)
{
    foc::Ampere phaseA{ 1.0f };
    foc::Ampere phaseB{ 2.0f };
    foc::Ampere phaseC{ 3.0f };

    onAdcMeasurementDone(phaseA, phaseB, phaseC);

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, adc_buffer_full_triggers_processing)
{
    foc::Ampere phaseA{ 1.0f };
    foc::Ampere phaseB{ 2.0f };
    foc::Ampere phaseC{ 3.0f };

    for (std::size_t i = 0; i < 100; ++i)
        onAdcMeasurementDone(phaseA, phaseB, phaseC);

    EXPECT_CALL(adcDecoratorMock, Stop()).Times(1);
    EXPECT_CALL(adcCreator, Destructed()).Times(1);
    EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());

    onAdcMeasurementDone(phaseA, phaseB, phaseC);

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, adc_reconfigure_after_buffer_full)
{
    foc::Ampere phaseA{ 1.0f };
    foc::Ampere phaseB{ 2.0f };
    foc::Ampere phaseC{ 3.0f };

    for (std::size_t i = 0; i < 100; ++i)
        onAdcMeasurementDone(phaseA, phaseB, phaseC);

    EXPECT_CALL(adcDecoratorMock, Stop()).Times(1);
    EXPECT_CALL(adcCreator, Destructed()).Times(1);
    EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());

    onAdcMeasurementDone(phaseA, phaseB, phaseC);

    ExecuteAllActions();

    InvokeCommand("adc medium", [this]()
        {
            EXPECT_CALL(adcCreator, Constructed(application::HardwareFactory::SampleAndHold::medium)).Times(1);
            EXPECT_CALL(adcDecoratorMock, Measure(testing::_)).WillOnce(testing::SaveArg<0>(&onAdcMeasurementDone));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, adc_multiple_samples_before_full)
{
    foc::Ampere phaseA1{ 1.0f };
    foc::Ampere phaseB1{ 2.0f };
    foc::Ampere phaseC1{ 3.0f };
    foc::Ampere phaseA2{ 4.0f };
    foc::Ampere phaseB2{ 5.0f };
    foc::Ampere phaseC2{ 6.0f };
    foc::Ampere phaseA3{ 7.0f };
    foc::Ampere phaseB3{ 8.0f };
    foc::Ampere phaseC3{ 9.0f };

    onAdcMeasurementDone(phaseA1, phaseB1, phaseC1);
    onAdcMeasurementDone(phaseA2, phaseB2, phaseC2);
    onAdcMeasurementDone(phaseA3, phaseB3, phaseC3);

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, encoder_command)
{
    EXPECT_CALL(encoderDecoratorMock, Read()).WillOnce(testing::Return(foc::Radians{ 1.57f }));

    InvokeCommand("enc", [this]()
        {
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, encoder_alias)
{
    EXPECT_CALL(encoderDecoratorMock, Read()).WillOnce(testing::Return(foc::Radians{ 3.14f }));

    InvokeCommand("e", [this]()
        {
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, encoder_returns_zero_position)
{
    EXPECT_CALL(encoderDecoratorMock, Read()).WillOnce(testing::Return(foc::Radians{ 0.0f }));

    InvokeCommand("enc", [this]()
        {
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, encoder_returns_negative_position)
{
    EXPECT_CALL(encoderDecoratorMock, Read()).WillOnce(testing::Return(foc::Radians{ -1.57f }));

    InvokeCommand("enc", [this]()
        {
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

// CAN command tests

TEST_F(TestHardwareTerminal, can_start_command)
{
    EXPECT_CALL(canCreator, Destructed()).Times(testing::AnyNumber());

    InvokeCommand("can_start 500000", [this]()
        {
            EXPECT_CALL(canCreator, Constructed(500000u, false));
            EXPECT_CALL(canAdapterMock, SetOnError(testing::_));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, can_start_with_test_mode)
{
    EXPECT_CALL(canCreator, Destructed()).Times(testing::AnyNumber());

    InvokeCommand("can_start 250000 test", [this]()
        {
            EXPECT_CALL(canCreator, Constructed(250000u, true));
            EXPECT_CALL(canAdapterMock, SetOnError(testing::_));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, can_start_invalid_argument_count)
{
    InvokeCommand("can_start", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid number of arguments" };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, can_start_invalid_bitrate)
{
    InvokeCommand("can_start 50000", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid bitrate. It should be between 125000 and 1000000." };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, can_start_invalid_option)
{
    InvokeCommand("can_start 500000 invalid", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid option. Use 'test' for loopback mode." };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, can_stop_command)
{
    EXPECT_CALL(canCreator, Destructed()).Times(testing::AnyNumber());

    InvokeCommand("can_start 500000", [this]()
        {
            EXPECT_CALL(canCreator, Constructed(500000u, false));
            EXPECT_CALL(canAdapterMock, SetOnError(testing::_));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();

    InvokeCommand("can_stop", [this]()
        {
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, can_send_without_start_returns_error)
{
    InvokeCommand("can_send 100 0x01 0x02", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "CAN not started. Run 'can_start' first." };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, can_listen_without_start_returns_error)
{
    InvokeCommand("can_listen", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "CAN not started. Run 'can_start' first." };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, can_send_calls_send_data)
{
    EXPECT_CALL(canCreator, Destructed()).Times(testing::AnyNumber());

    InvokeCommand("can_start 500000", [this]()
        {
            EXPECT_CALL(canCreator, Constructed(500000u, false));
            EXPECT_CALL(canAdapterMock, SetOnError(testing::_));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();

    InvokeCommand("can_send 100 1 2 3", [this]()
        {
            EXPECT_CALL(canAdapterMock, SendData(testing::_, testing::_, testing::_));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, can_send_invalid_argument_count)
{
    EXPECT_CALL(canCreator, Destructed()).Times(testing::AnyNumber());

    InvokeCommand("can_start 500000", [this]()
        {
            EXPECT_CALL(canCreator, Constructed(500000u, false));
            EXPECT_CALL(canAdapterMock, SetOnError(testing::_));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();

    InvokeCommand("can_send", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid number of arguments. Usage: can_send <id> <b0> ... <b7>" };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, can_listen_calls_receive_data)
{
    EXPECT_CALL(canCreator, Destructed()).Times(testing::AnyNumber());

    InvokeCommand("can_start 500000", [this]()
        {
            EXPECT_CALL(canCreator, Constructed(500000u, false));
            EXPECT_CALL(canAdapterMock, SetOnError(testing::_));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();

    InvokeCommand("can_listen", [this]()
        {
            EXPECT_CALL(canAdapterMock, ReceiveData(testing::_));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, can_send_after_stop_returns_error)
{
    EXPECT_CALL(canCreator, Destructed()).Times(testing::AnyNumber());

    InvokeCommand("can_start 500000", [this]()
        {
            EXPECT_CALL(canCreator, Constructed(500000u, false));
            EXPECT_CALL(canAdapterMock, SetOnError(testing::_));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();

    InvokeCommand("can_stop", [this]()
        {
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();

    InvokeCommand("can_send 100 1", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "CAN not started. Run 'can_start' first." };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, can_send_invalid_id_returns_error)
{
    EXPECT_CALL(canCreator, Destructed()).Times(testing::AnyNumber());

    InvokeCommand("can_start 500000", [this]()
        {
            EXPECT_CALL(canCreator, Constructed(500000u, false));
            EXPECT_CALL(canAdapterMock, SetOnError(testing::_));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();

    InvokeCommand("can_send abc 1", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid CAN ID. It should be between 0 and 0x1FFFFFFF." };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, can_send_invalid_data_byte_returns_error)
{
    EXPECT_CALL(canCreator, Destructed()).Times(testing::AnyNumber());

    InvokeCommand("can_start 500000", [this]()
        {
            EXPECT_CALL(canCreator, Constructed(500000u, false));
            EXPECT_CALL(canAdapterMock, SetOnError(testing::_));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();

    InvokeCommand("can_send 100 999", [this]()
        {
            ::testing::InSequence _;

            std::string newline{ "\r\n" };
            std::string header{ "ERROR: " };
            std::string payload{ "invalid data byte. It should be between 0 and 255." };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TestHardwareTerminal, can_send_success_callback_traces_message)
{
    EXPECT_CALL(canCreator, Destructed()).Times(testing::AnyNumber());

    InvokeCommand("can_start 500000", [this]()
        {
            EXPECT_CALL(canCreator, Constructed(500000u, false));
            EXPECT_CALL(canAdapterMock, SetOnError(testing::_));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();

    infra::Function<void(bool)> sendCallback;

    InvokeCommand("can_send 100 1 2", [this, &sendCallback]()
        {
            EXPECT_CALL(canAdapterMock, SendData(testing::_, testing::_, testing::_))
                .WillOnce(testing::SaveArg<2>(&sendCallback));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();

    EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
    sendCallback(true);
}

TEST_F(TestHardwareTerminal, can_send_failure_callback_traces_message)
{
    EXPECT_CALL(canCreator, Destructed()).Times(testing::AnyNumber());

    InvokeCommand("can_start 500000", [this]()
        {
            EXPECT_CALL(canCreator, Constructed(500000u, false));
            EXPECT_CALL(canAdapterMock, SetOnError(testing::_));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();

    infra::Function<void(bool)> sendCallback;

    InvokeCommand("can_send 100 1 2", [this, &sendCallback]()
        {
            EXPECT_CALL(canAdapterMock, SendData(testing::_, testing::_, testing::_))
                .WillOnce(testing::SaveArg<2>(&sendCallback));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();

    EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
    sendCallback(false);
}

TEST_F(TestHardwareTerminal, can_listen_receive_callback_traces_11bit_message)
{
    EXPECT_CALL(canCreator, Destructed()).Times(testing::AnyNumber());

    InvokeCommand("can_start 500000", [this]()
        {
            EXPECT_CALL(canCreator, Constructed(500000u, false));
            EXPECT_CALL(canAdapterMock, SetOnError(testing::_));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();

    infra::Function<void(hal::Can::Id, const hal::Can::Message&)> receiveCallback;

    InvokeCommand("can_listen", [this, &receiveCallback]()
        {
            EXPECT_CALL(canAdapterMock, ReceiveData(testing::_))
                .WillOnce(testing::SaveArg<0>(&receiveCallback));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();

    hal::Can::Message msg;
    msg.push_back(0xAA);
    msg.push_back(0xBB);

    EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
    receiveCallback(hal::Can::Id::Create11BitId(0x100), msg);
}

TEST_F(TestHardwareTerminal, can_listen_receive_callback_traces_29bit_message)
{
    EXPECT_CALL(canCreator, Destructed()).Times(testing::AnyNumber());

    InvokeCommand("can_start 500000", [this]()
        {
            EXPECT_CALL(canCreator, Constructed(500000u, false));
            EXPECT_CALL(canAdapterMock, SetOnError(testing::_));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();

    infra::Function<void(hal::Can::Id, const hal::Can::Message&)> receiveCallback;

    InvokeCommand("can_listen", [this, &receiveCallback]()
        {
            EXPECT_CALL(canAdapterMock, ReceiveData(testing::_))
                .WillOnce(testing::SaveArg<0>(&receiveCallback));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();

    hal::Can::Message msg;
    msg.push_back(0xCC);

    EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
    receiveCallback(hal::Can::Id::Create29BitId(0x1ABCDEF), msg);
}

TEST_F(TestHardwareTerminal, can_error_callback_traces_error)
{
    EXPECT_CALL(canCreator, Destructed()).Times(testing::AnyNumber());

    infra::Function<void(application::CanBusAdapter::CanError)> errorCallback;

    InvokeCommand("can_start 500000", [this, &errorCallback]()
        {
            EXPECT_CALL(canCreator, Constructed(500000u, false));
            EXPECT_CALL(canAdapterMock, SetOnError(testing::_))
                .WillOnce(testing::SaveArg<0>(&errorCallback));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();

    EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
    errorCallback(application::CanBusAdapter::CanError::busOff);
}

TEST_F(TestHardwareTerminal, can_send_with_29bit_id)
{
    EXPECT_CALL(canCreator, Destructed()).Times(testing::AnyNumber());

    InvokeCommand("can_start 500000", [this]()
        {
            EXPECT_CALL(canCreator, Constructed(500000u, false));
            EXPECT_CALL(canAdapterMock, SetOnError(testing::_));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();

    InvokeCommand("can_send 2048 1", [this]()
        {
            EXPECT_CALL(canAdapterMock, SendData(hal::Can::Id::Create29BitId(2048), testing::_, testing::_));
            EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AnyNumber());
        });

    ExecuteAllActions();
}
