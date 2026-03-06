#pragma once

#include "source/simulator/model/Model.hpp"
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace simulator
{
    struct AngleUnit
    {
        std::string label = "Electrical Angle [rad]";
        float scaleFactor = 1.0f;
    };

    class Plot
        : public ThreePhaseMotorModelObserver
    {
    public:
        Plot(ThreePhaseMotorModel& model, const std::string& title, const std::string& filename, const std::filesystem::path& outputDirectory,
            std::chrono::microseconds timeStep, std::chrono::milliseconds simulationTime, const AngleUnit& angleUnit = {});

        // Implementation of ThreePhaseMotorModelObserver
        void Started() override;
        void PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currentPhases, foc::Radians theta) override;
        void Finished() override;

    private:
        std::string title;
        std::string filename;
        std::filesystem::path outputDirectory;
        std::chrono::microseconds timeStep;
        AngleUnit angleUnit;
        std::vector<float> time;
        std::vector<float> i_a;
        std::vector<float> i_b;
        std::vector<float> i_c;
        std::vector<float> theta;
    };
}
