#pragma once

#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "core/foc/interfaces/Driver.hpp"
#include "tools/simulator/model/Model.hpp"
#include "tools/simulator/view/plot/Plot.hpp"
#include <chrono>
#include <string>

namespace simulator
{
    struct HeadlessSimulationConfig
    {
        std::string title;
        std::string filename;
        std::string outputDirectory;
        std::chrono::microseconds timeStep;
        std::chrono::milliseconds simulationTime;
        AngleUnit angleUnit{};
    };

    class HeadlessSimulation
    {
    public:
        HeadlessSimulation(ThreePhaseMotorModel& model, foc::Controllable& controller, infra::EventDispatcherWithWeakPtr& eventDispatcher,
            const HeadlessSimulationConfig& config);

        void Run();

    private:
        foc::Controllable& controller;
        infra::EventDispatcherWithWeakPtr& eventDispatcher;

        Plot plotter;
        bool simulationFinished = false;
        SimulationFinishedObserver finishedObserver;
    };
}
