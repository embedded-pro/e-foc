#include "tools/simulator/headless/HeadlessSimulation.hpp"

namespace simulator
{
    HeadlessSimulation::HeadlessSimulation(ThreePhaseMotorModel& model, foc::Controllable& controller, infra::EventDispatcherWithWeakPtr& eventDispatcher,
        const HeadlessSimulationConfig& config)
        : controller(controller)
        , eventDispatcher(eventDispatcher)
        , plotter(model, config.title, config.filename, config.outputDirectory, config.timeStep, config.simulationTime, config.angleUnit)
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
