#pragma once

#include "source/simulator/model/Model.hpp"
#include <chrono>
#include <sciplot/sciplot.hpp>
#include <vector>

namespace simulator
{
    class Plot
        : public ThreePhaseMotorModelObserver
    {
    public:
        struct Currents
        {
            std::vector<float> i_a;
            std::vector<float> i_b;
            std::vector<float> i_c;
        };

        Plot(ThreePhaseMotorModel& model, const std::string& title, const std::string& filename, std::chrono::microseconds timeStep, std::chrono::milliseconds simulationTime);

        // Implementation of ThreePhaseMotorModelObserver
        void PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currentPhases, foc::Radians theta) override;
        void Finished() override;

    private:
        ThreePhaseMotorModel& model;
        std::string title;
        std::string filename;
        std::chrono::microseconds timeStep;
        std::chrono::milliseconds simulationTime;
        std::vector<float> time;
        std::vector<float> i_a;
        std::vector<float> i_b;
        std::vector<float> i_c;
        std::vector<float> theta;
    };
}
