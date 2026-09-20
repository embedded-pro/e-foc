#include "TestFocStateMachineHelper.hpp"
#include "infra/stream/StringOutputStream.hpp"
#include "services/fsm/StateMachineMermaid.hpp"
#include <array>
#include <string>

namespace
{
    using namespace testing;

    using StateId = services::AlternativeId<state_machine::State>;
    using EventId = services::AlternativeId<state_machine::Event>;

    template<class S>
    constexpr StateId State()
    {
        return StateId::Of<S>();
    }

    template<class E>
    constexpr EventId Event()
    {
        return EventId::Of<E>();
    }

    struct Edge
    {
        StateId from;
        EventId event;
    };

    constexpr std::array documentedCommandEdges{
        Edge{ State<state_machine::Idle>(), Event<state_machine::Calibrate>() },
        Edge{ State<state_machine::Idle>(), Event<state_machine::ReAlign>() },
        Edge{ State<state_machine::Idle>(), Event<state_machine::ReserveExternalCalibration>() },
        Edge{ State<state_machine::Idle>(), Event<state_machine::ClearCalibration>() },
        Edge{ State<state_machine::Idle>(), Event<state_machine::SetFluxLinkage>() },
        Edge{ State<state_machine::Ready>(), Event<state_machine::Calibrate>() },
        Edge{ State<state_machine::Ready>(), Event<state_machine::ReAlign>() },
        Edge{ State<state_machine::Ready>(), Event<state_machine::ReserveExternalCalibration>() },
        Edge{ State<state_machine::Ready>(), Event<state_machine::ClearCalibration>() },
        Edge{ State<state_machine::Ready>(), Event<state_machine::SetFluxLinkage>() },
        Edge{ State<state_machine::Ready>(), Event<state_machine::Enable>() },
        Edge{ State<state_machine::Calibrating>(), Event<state_machine::CompleteExternalCalibration>() },
        Edge{ State<state_machine::Enabled>(), Event<state_machine::Disable>() },
        Edge{ State<state_machine::Fault>(), Event<state_machine::ClearFault>() },
    };

    constexpr std::array commandEvents{
        Event<state_machine::Calibrate>(),
        Event<state_machine::ReAlign>(),
        Event<state_machine::ReserveExternalCalibration>(),
        Event<state_machine::CompleteExternalCalibration>(),
        Event<state_machine::ClearCalibration>(),
        Event<state_machine::SetFluxLinkage>(),
        Event<state_machine::Enable>(),
        Event<state_machine::Disable>(),
        Event<state_machine::ClearFault>(),
    };

    struct Completion
    {
        EventId event;
        std::array<bool, StateId::count> handledIn;
    };

    constexpr std::array<bool, StateId::count> onlyIdle{ true, false, false, false, false };
    constexpr std::array<bool, StateId::count> onlyCalibrating{ false, true, false, false, false };
    constexpr std::array<bool, StateId::count> idleAndReady{ true, false, true, false, false };
    constexpr std::array<bool, StateId::count> everywhere{ true, true, true, true, true };

    constexpr std::array serviceCompletions{
        Completion{ Event<state_machine::CalibrationStepChanged>(), onlyCalibrating },
        Completion{ Event<state_machine::AlignmentSucceeded>(), onlyCalibrating },
        Completion{ Event<state_machine::CalibrationStepFailed>(), onlyCalibrating },
        Completion{ Event<state_machine::MechanicalParametersIdentified>(), onlyCalibrating },
        Completion{ Event<state_machine::CalibrationSaved>(), onlyCalibrating },
        Completion{ Event<state_machine::RunCalibrationSequence>(), onlyCalibrating },
        Completion{ Event<state_machine::RunAlignmentOnly>(), onlyCalibrating },
        Completion{ Event<state_machine::CalibrationInvalidated>(), idleAndReady },
        Completion{ Event<state_machine::BootValidityChecked>(), onlyIdle },
        Completion{ Event<state_machine::BootCalibrationLoaded>(), onlyIdle },
        Completion{ Event<state_machine::FluxLinkageSaved>(), idleAndReady },
        Completion{ Event<state_machine::FaultDetected>(), everywhere },
        Completion{ Event<state_machine::EmergencyStop>(), everywhere },
    };

    class FocStateMachineTableTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        bool HasTransition(StateId from, EventId event) const
        {
            return stateMachine.TransitionTable().HasTransition(from, event);
        }

        StrictMock<infra::StreamWriterMock> streamWriterMock;
        infra::TextOutputStream::WithErrorPolicy stream{ streamWriterMock };
        services::TracerToStream tracer{ stream };
        testing::StrictMock<hal::SerialCommunicationMock> communication;
        infra::Execute setupStreamExpectations{ [this]()
            {
                EXPECT_CALL(streamWriterMock, Insert(_, _)).Times(AnyNumber());
                EXPECT_CALL(streamWriterMock, Available()).Times(AnyNumber()).WillRepeatedly(Return(1000));
                EXPECT_CALL(communication, SendDataMock(_)).Times(AnyNumber());
            } };
        services::TerminalWithCommandsImpl::WithMaxQueueAndMaxHistory<128, 5> terminalWithCommands{ communication, tracer };
        services::TerminalWithStorage::WithMaxSize<20> terminal{ terminalWithCommands, tracer };

        StrictMock<drivers::ThreePhaseInverterMock> inverterMock;
        StrictMock<drivers::EncoderMock> encoderMock;
        StrictMock<services::NonVolatileMemoryMock> nvmMock;
        StrictMock<services::ElectricalParametersIdentificationMock> electricalIdentMock;
        StrictMock<services::MotorAlignmentMock> alignmentMock;
        StrictMock<state_machine::FaultNotifierMock> faultNotifierMock;
        infra::Execute setupMachineExpectations{ [this]()
            {
                EXPECT_CALL(nvmMock, IsCalibrationValid(_)).WillOnce(Invoke([](infra::Function<void(bool)> onDone)
                    {
                        onDone(false);
                    }));
                EXPECT_CALL(faultNotifierMock, Register(_, _));
                EXPECT_CALL(faultNotifierMock, Unregister());
                EXPECT_CALL(electricalIdentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(alignmentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(inverterMock, BaseFrequency()).Times(AnyNumber()).WillRepeatedly(Return(hal::Hertz{ 10000 }));
                EXPECT_CALL(inverterMock, PhaseCurrentsReady(_, _)).Times(AnyNumber());
                EXPECT_CALL(inverterMock, Stop()).Times(AnyNumber());
            } };

        application::TorqueStateMachine stateMachine{
            application::TerminalAndTracer{ terminal, tracer },
            application::MotorHardware{ inverterMock, encoderMock, foc::Volts{ 24.0f } },
            nvmMock,
            application::CalibrationServices{ electricalIdentMock, alignmentMock },
            faultNotifierMock,
            state_machine::TransitionPolicy::Auto
        };
    };
}

TEST_F(FocStateMachineTableTest, table_is_consistent_from_idle)
{
    EXPECT_EQ(services::ConsistencyError::none, stateMachine.TransitionTable().CheckConsistency<state_machine::Idle>());
}

TEST_F(FocStateMachineTableTest, commands_are_accepted_exactly_where_the_design_documents_them)
{
    for (std::size_t state = 0; state != StateId::count; ++state)
        for (auto event : commandEvents)
        {
            bool documented = false;
            for (const auto& edge : documentedCommandEdges)
                documented = documented || (edge.from == StateId::FromIndex(state) && edge.event == event);

            EXPECT_EQ(documented, HasTransition(StateId::FromIndex(state), event)) << event.Name() << " in " << StateId::FromIndex(state).Name();
        }
}

TEST_F(FocStateMachineTableTest, service_completions_are_handled_only_in_the_state_that_issued_them)
{
    for (const auto& completion : serviceCompletions)
        for (std::size_t state = 0; state != StateId::count; ++state)
            EXPECT_EQ(completion.handledIn[state], HasTransition(StateId::FromIndex(state), completion.event)) << completion.event.Name() << " in " << StateId::FromIndex(state).Name();
}

TEST_F(FocStateMachineTableTest, every_event_is_covered_by_the_documented_sets)
{
    for (std::size_t event = 0; event != EventId::count; ++event)
    {
        bool covered = false;
        for (auto command : commandEvents)
            covered = covered || command == EventId::FromIndex(event);
        for (const auto& completion : serviceCompletions)
            covered = covered || completion.event == EventId::FromIndex(event);

        EXPECT_TRUE(covered) << EventId::FromIndex(event).Name();
    }
}

TEST_F(FocStateMachineTableTest, mermaid_export_of_the_table_is_available_for_the_documentation)
{
    infra::StringOutputStream::WithStorage<4096> mermaid;
    services::WriteMermaid(mermaid, stateMachine.TransitionTable(), State<state_machine::Idle>());
    const std::string diagram{ mermaid.Storage().begin(), mermaid.Storage().end() };

    EXPECT_THAT(diagram, HasSubstr("stateDiagram-v2\n    [*] --> Idle\n"));
    EXPECT_THAT(diagram, HasSubstr("    Ready --> Enabled : Enable [guarded]\n"));
}
