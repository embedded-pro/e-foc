#include "source/simulator/headless/HeadlessSimulation.hpp"

namespace simulator
{
    HeadlessSimulation::HeadlessSimulation(ThreePhaseMotorModel& model, foc::Controllable& controller, infra::EventDispatcherWithWeakPtr& eventDispatcher,
        const std::string& title, const std::string& filename, const std::string& outputDirectory,
        std::chrono::microseconds timeStep, std::chrono::milliseconds simulationTime)
        : controller(controller)
        , eventDispatcher(eventDispatcher)
        , plotter(model, title, filename, outputDirectory, timeStep, simulationTime)
        , finishedObserver(model, [this]()
              {
                  simulationFinished = true;
              })
    {}

    void HeadlessSimulation::Run()
    {
        controller.Start();

        eventDispatcher.ExecuteUntil([this]()
            {
                return simulationFinished;
            });
    }
}
