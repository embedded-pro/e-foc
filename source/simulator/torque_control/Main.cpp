#include "foc/interfaces/FieldOrientedController.hpp"
#include "simulator/model/Jk42bls01X038ed.hpp"
#include "simulator/model/Model.hpp"
#include "simulator/plot/Plot.hpp"
#include "source/foc/instantiations/FieldOrientedControllerImpl.hpp"
#include "source/foc/interfaces/Driver.hpp"
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sciplot/sciplot.hpp>
#include <vector>

namespace
{
    class Simulator
        : public simulator::ThreePhaseMotorModelObserver
    {
    public:
        Simulator()
        {
            Attach(model);
        }

        ~Simulator()
        {
            Detach();
        }

        void PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currentPhases, foc::Radians theta) override
        {
            if (time.size() < steps)
            {
                StoreData(currentPhases, theta);

                if (time.size() % (steps / 10) == 0)
                    PrintProgress();
            }
            else
                FinishSimulation();
        }

        void StoreData(foc::PhaseCurrents currentPhases, foc::Radians theta)
        {
            auto [i_a, i_b, i_c] = currentPhases;
            auto currentTime = static_cast<float>(time.size()) * std::chrono::duration_cast<std::chrono::duration<float>>(timeStep).count();

            time.push_back(currentTime);
            i_a_data.push_back(i_a.Value());
            i_b_data.push_back(i_b.Value());
            i_c_data.push_back(i_c.Value());
            theta_data.push_back(theta.Value());
        }

        void PrintProgress()
        {
            float progress = 100.0f * static_cast<float>(time.size()) / static_cast<float>(steps);
            std::cout << "Progress: " << std::fixed << std::setprecision(0) << progress << "%\n";
        }

        void FinishSimulation()
        {
            focTorque.Disable();
            model.Stop();

            graphics::PlotResults plotter{ "FOC Torque Control", "foc_torque_results" };
            plotter.Save(time, { i_a_data, i_b_data, i_c_data }, theta_data);
            std::cout << "\nSimulation completed!\n";
        }

        void Run()
        {
            float Kp_current = 15.0f;
            float Ki_current = 2000.0f;
            float Kd_current = 0.0f;

            float Id_setpoint = 0.0f;
            float Iq_setpoint = 0.5f;

            std::cout << "Motor Parameters:\n";
            std::cout << "  R = " << simulator::JK42BLS01_X038ED::parameters.R.Value() << " Ω\n";
            std::cout << "  L = " << simulator::JK42BLS01_X038ED::parameters.Ld.Value() * 1000.0f << " mH\n";
            std::cout << "  Vdc = " << simulator::JK42BLS01_X038ED::parameters.Vdc.Value() << " V\n";
            std::cout << "  Pole pairs = " << simulator::JK42BLS01_X038ED::parameters.p << "\n";
            std::cout << "  Max current = 15 A\n";
            std::cout << "\nPID Tuning:\n";
            std::cout << "Current Loop (Id/Iq):\n";
            std::cout << "  Kp = " << Kp_current << "\n";
            std::cout << "  Ki = " << Ki_current << "\n";
            std::cout << "  Kd = " << Kd_current << "\n";
            std::cout << "  Output limit = ±1.0 (normalized modulation)\n";
            std::cout << "\nTorque Control Setpoint:\n";
            std::cout << "  Id = " << Id_setpoint << " A (field weakening)\n";
            std::cout << "  Iq = " << Iq_setpoint << " A (torque)\n\n";

            time.reserve(steps);
            i_a_data.reserve(steps);
            i_b_data.reserve(steps);
            i_c_data.reserve(steps);
            theta_data.reserve(steps);

            focTorque.SetPolePairs(static_cast<std::size_t>(simulator::JK42BLS01_X038ED::parameters.p));
            focTorque.SetCurrentTunings(
                simulator::JK42BLS01_X038ED::parameters.Vdc,
                foc::IdAndIqTunings{
                    { Kp_current, Ki_current, Kd_current },
                    { Kp_current, Ki_current, Kd_current } });
            focTorque.SetPoint(foc::IdAndIqPoint{ foc::Ampere{ Id_setpoint }, foc::Ampere{ Iq_setpoint } });

            std::cout << "Starting simulation...\n";
            std::cout << "Duration: " << simulationTime.count() << " ms\n";
            std::cout << "Time step: " << timeStep.count() << " μs\n";
            std::cout << "Total steps: " << steps << "\n";

            foc::PhaseCurrents latestCurrents{ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } };
            model.PhaseCurrentsReady(hal::Hertz{ 100000 }, [&latestCurrents](foc::PhaseCurrents currents)
                {
                    latestCurrents = currents;
                });

            focTorque.Enable();
            model.Start();

            for (std::size_t i = 0; i < steps; ++i)
            {
                auto angle = model.Read();
                auto duties = focTorque.Calculate(latestCurrents, angle);
                model.ThreePhasePwmOutput(duties);
            }

            FinishSimulation();
        }

    private:
        static constexpr std::chrono::microseconds timeStep{ 10 };
        static constexpr std::chrono::milliseconds simulationTime{ 1000 };
        static constexpr auto steps = static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::duration<float>>(simulationTime).count() / std::chrono::duration_cast<std::chrono::duration<float>>(timeStep).count());

        std::vector<float> time;
        std::vector<float> i_a_data;
        std::vector<float> i_b_data;
        std::vector<float> i_c_data;
        std::vector<float> theta_data;

        simulator::ThreePhaseMotorModel model{ simulator::JK42BLS01_X038ED::parameters };
        foc::Foc
    };
}

int main()
{
    Simulator simulator;
    simulator.Run();

    return 0;
}
