#pragma once

#include "foc/implementations/Runner.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "source/simulator/model/Model.hpp"
#include "source/simulator/view/plot/Plot.hpp"
#include <chrono>
#include <string>

namespace simulator
{
    class HeadlessSimulation
    {
    public:
        HeadlessSimulation(ThreePhaseMotorModel& model, foc::Runner& runner, infra::EventDispatcherWithWeakPtr& eventDispatcher,
            const std::string& title, const std::string& filename, const std::string& outputDirectory,
            std::chrono::microseconds timeStep, std::chrono::milliseconds simulationTime);

        void Run();

    private:
        foc::Runner& runner;
        infra::EventDispatcherWithWeakPtr& eventDispatcher;

        Plot plotter;
        bool simulationFinished = false;
        SimulationFinishedObserver finishedObserver;
    };
}
