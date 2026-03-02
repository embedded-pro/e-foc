#include "source/simulator/headless/HeadlessSimulation.hpp"
#include "foc/implementations/Runner.hpp"

namespace simulator
{
    HeadlessSimulation::HeadlessSimulation(ThreePhaseMotorModel& model, foc::Runner& runner, infra::EventDispatcherWithWeakPtr& eventDispatcher,
        const std::string& title, const std::string& filename, const std::string& outputDirectory,
        std::chrono::microseconds timeStep, std::chrono::milliseconds simulationTime)
        : runner(runner)
        , eventDispatcher(eventDispatcher)
        , plotter(model, title, filename, outputDirectory, timeStep, simulationTime)
        , finishedObserver(model, [this]()
              {
                  simulationFinished = true;
              })
    {}

    void HeadlessSimulation::Run()
    {
        runner.Enable();

        eventDispatcher.ExecuteUntil([this]()
            {
                return simulationFinished;
            });
    }
}
