#pragma once

#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "source/foc/interfaces/Driver.hpp"
#include "source/tool/simulator/model/Model.hpp"
#include "source/tool/simulator/view/plot/Plot.hpp"
#include <chrono>
#include <string>

namespace simulator
{
    class HeadlessSimulation
    {
    public:
        HeadlessSimulation(ThreePhaseMotorModel& model, foc::Controllable& controller, infra::EventDispatcherWithWeakPtr& eventDispatcher,
            const std::string& title, const std::string& filename, const std::string& outputDirectory,
            std::chrono::microseconds timeStep, std::chrono::milliseconds simulationTime, const AngleUnit& angleUnit = {});

        void Run();

    private:
        foc::Controllable& controller;
        infra::EventDispatcherWithWeakPtr& eventDispatcher;

        Plot plotter;
        bool simulationFinished = false;
        SimulationFinishedObserver finishedObserver;
    };
}
