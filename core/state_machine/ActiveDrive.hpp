#pragma once

#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "core/state_machine/ControlMode.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "core/state_machine/OuterLoopStateMachine.hpp"
#include "core/state_machine/PositionStateMachine.hpp"
#include "core/state_machine/SpeedStateMachine.hpp"
#include "core/state_machine/TorqueStateMachine.hpp"
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace state_machine
{
    class ActiveDrive
    {
    public:
        explicit ActiveDrive(const services::ConfigData& config);

        ControlMode Active() const;
        bool HasMachine() const;

        template<class Machine, class... Args>
        Machine& Emplace(Args&&... args)
        {
            return machines.emplace<Machine>(std::forward<Args>(args)...);
        }

        template<class Visitor>
        void Visit(Visitor&& visitor)
        {
            std::visit(std::forward<Visitor>(visitor), machines);
        }

        FocStateMachineBase& ActiveStateMachine();
        const FocStateMachineBase& ActiveStateMachine() const;
        application::FocStateMachineCommon& ActiveCommon();
        const application::FocStateMachineCommon& ActiveCommon() const;
        application::OuterLoopStateMachine* ActiveOuterLoop();
        const application::OuterLoopStateMachine* ActiveOuterLoop() const;

        template<class Interface>
        Interface* Controller()
        {
            return std::visit([]<class Machine>(Machine& machine) -> Interface*
                {
                    if constexpr (std::is_same_v<Machine, std::monostate>)
                        return nullptr;
                    else if constexpr (std::is_convertible_v<std::remove_reference_t<decltype(machine.GetController())>*, Interface*>)
                        return &machine.GetController();
                    else
                        return nullptr;
                },
                machines);
        }

        template<class Interface>
        const Interface* Controller() const
        {
            return std::visit([]<class Machine>(const Machine& machine) -> const Interface*
                {
                    if constexpr (std::is_same_v<Machine, std::monostate>)
                        return nullptr;
                    else if constexpr (std::is_convertible_v<std::remove_reference_t<decltype(machine.GetController())>*, const Interface*>)
                        return &machine.GetController();
                    else
                        return nullptr;
                },
                machines);
        }

        application::RotorReference RotorReferenceToKeep() const;
        const services::CalibrationData& CalibrationRecord() const;
        std::optional<services::CalibrationData> ActiveCalibrationData() const;
        bool IsHomed() const;
        std::optional<foc::Celsius> WindingTemperature();

    private:
        const services::ConfigData& config;
        std::variant<std::monostate,
            application::TorqueStateMachine,
            application::SpeedStateMachine,
            application::PositionStateMachine>
            machines;
    };
}
