#pragma once

#include "core/foc/interfaces/Driver.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "tools/simulator/model/Model.hpp"

namespace simulator
{
    class HeadlessSimulation
    {
    public:
        HeadlessSimulation(ThreePhaseMotorModel& model, foc::Controllable& controller, infra::EventDispatcherWithWeakPtr& eventDispatcher);

        void Run();

    private:
        foc::Controllable& controller;
        infra::EventDispatcherWithWeakPtr& eventDispatcher;

        bool simulationFinished = false;
        SimulationFinishedObserver finishedObserver;
    };
}
