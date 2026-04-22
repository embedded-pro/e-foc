#pragma once

#include "core/foc/interfaces/Driver.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "tools/simulator/model/Model.hpp"
#include <chrono>
#include <string>

namespace simulator
{
    struct AngleUnit
    {
        std::string label = "Electrical Angle [rad]";
        float scaleFactor = 1.0f;
    };

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

        bool simulationFinished = false;
        SimulationFinishedObserver finishedObserver;
    };
}
