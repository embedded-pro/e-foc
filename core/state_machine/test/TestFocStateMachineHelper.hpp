#pragma once

#include "core/foc/implementations/test_doubles/DriversMock.hpp"
#include "core/services/alignment/test_doubles/MotorAlignmentMock.hpp"
#include "core/services/cli/TerminalTorque.hpp"
#include "core/services/electrical_system_ident/test_doubles/ElectricalParametersIdentificationMock.hpp"
#include "core/services/mechanical_system_ident/test_doubles/MechanicalParametersIdentificationMock.hpp"
#include "core/services/non_volatile_memory/test_doubles/NonVolatileMemoryMock.hpp"
#include "core/state_machine/FocStateMachineImpl.hpp"
#include "core/state_machine/test_doubles/FaultNotifierMock.hpp"
#include "hal/interfaces/test_doubles/SerialCommunicationMock.hpp"
#include "infra/event/test_helper/EventDispatcherWithWeakPtrFixture.hpp"
#include "infra/stream/test/StreamMock.hpp"
#include "infra/util/Function.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/Terminal.hpp"
#include <gmock/gmock.h>
