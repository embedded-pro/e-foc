#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "core/foc/cascade/PositionCascade.hpp"
#include "core/foc/cascade/SpeedCascade.hpp"
#include "core/foc/cascade/TorqueCascade.hpp"
#include "core/foc/interfaces/Execution.hpp"
#include "core/foc/interfaces/MotorModel.hpp"
#include "core/foc/interfaces/Signals.hpp"
#include "hal/cortex_m/DataWatchpointAndTrace.hpp"
#include "infra/util/Function.hpp"
#include "integration_tests/software_in_the_loop/qemu_target/SilQemuApplication.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
    constexpr uint32_t warmupIterations = 50;
    constexpr uint32_t measuredIterations = 200;

    uint32_t BenchmarkTorque(foc::TorqueCascade& cascade, hal::cortex::DataWatchpointAndTrace& dwt)
    {
        const foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
        foc::Radians position{ 0.0f };
        uint32_t minCycles = UINT32_MAX;

        for (uint32_t i = 0; i < warmupIterations; ++i)
        {
            cascade.Calculate(currents, position);
            position += foc::Radians{ 0.01f };
        }

        for (uint32_t i = 0; i < measuredIterations; ++i)
        {
            dwt.Start();
            cascade.Calculate(currents, position);
            dwt.Stop();
            minCycles = std::min(minCycles, dwt.Cycles());
            position += foc::Radians{ 0.01f };
        }

        return minCycles;
    }

    uint32_t BenchmarkSpeed(foc::SpeedCascade& cascade, hal::cortex::DataWatchpointAndTrace& dwt)
    {
        const foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
        foc::Radians position{ 0.0f };
        uint32_t minCycles = UINT32_MAX;

        for (uint32_t i = 0; i < warmupIterations; ++i)
        {
            cascade.Calculate(currents, position);
            position += foc::Radians{ 0.01f };
        }

        for (uint32_t i = 0; i < measuredIterations; ++i)
        {
            dwt.Start();
            cascade.Calculate(currents, position);
            dwt.Stop();
            minCycles = std::min(minCycles, dwt.Cycles());
            position += foc::Radians{ 0.01f };
        }

        return minCycles;
    }

    uint32_t BenchmarkPosition(foc::PositionCascade& cascade, hal::cortex::DataWatchpointAndTrace& dwt)
    {
        const foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
        foc::Radians position{ 0.0f };
        uint32_t minCycles = UINT32_MAX;

        for (uint32_t i = 0; i < warmupIterations; ++i)
        {
            cascade.Calculate(currents, position);
            position += foc::Radians{ 0.01f };
        }

        for (uint32_t i = 0; i < measuredIterations; ++i)
        {
            dwt.Start();
            cascade.Calculate(currents, position);
            dwt.Stop();
            minCycles = std::min(minCycles, dwt.Cycles());
            position += foc::Radians{ 0.01f };
        }

        return minCycles;
    }

    void RunDwtBenchmark()
    {
        hal::cortex::DataWatchpointAndTrace dwt;

        const foc::MotorModelParameters motorParams{
            foc::Ohm{ 0.073f },
            foc::MilliHenry{ 0.5f },
            foc::Weber{ 0.007f },
            foc::Volts{ 48.0f },
            hal::Hertz{ 20000 },
            4
        };

        const foc::MechanicalModelParameters mechParams{
            foc::NewtonMeterSecondSquared{ 0.0000075f },
            foc::NewtonMeterSecondPerRadian{ 0.00002f },
            foc::NewtonMeter{ 0.028f },
            foc::Ampere{ 15.0f },
            hal::Hertz{ 1000 }
        };

        struct NoOpLpi : foc::LowPriorityInterrupt
        {
            void Trigger() override {}
            void Register(const infra::Function<void()>&) override {}
            void Unregister() override {}
        } lowPriInterrupt;

        foc::TorqueCascade torque;
        torque.Configure(motorParams);
        torque.Enable();

        foc::SpeedCascade speed{ foc::Ampere{ 15.0f }, hal::Hertz{ 20000 }, lowPriInterrupt };
        speed.Configure(motorParams);
        speed.ConfigureMechanics(mechParams);
        speed.Enable();

        foc::PositionCascade position{ foc::Ampere{ 15.0f }, hal::Hertz{ 20000 }, lowPriInterrupt };
        position.Configure(motorParams);
        position.ConfigureMechanics(mechParams);
        position.Enable();

        std::printf("READY\n");
        std::fflush(stdout);

        const uint32_t torqueCycles = BenchmarkTorque(torque, dwt);
        const uint32_t speedCycles = BenchmarkSpeed(speed, dwt);
        const uint32_t positionCycles = BenchmarkPosition(position, dwt);

        std::printf("DWT torque_calculate=%lu\n", static_cast<unsigned long>(torqueCycles));
        std::printf("DWT speed_calculate=%lu\n", static_cast<unsigned long>(speedCycles));
        std::printf("DWT position_calculate=%lu\n", static_cast<unsigned long>(positionCycles));
        std::printf("[[CYCLES]] torque_calculate=%lu speed_calculate=%lu position_calculate=%lu\n",
            static_cast<unsigned long>(torqueCycles),
            static_cast<unsigned long>(speedCycles),
            static_cast<unsigned long>(positionCycles));
        std::printf("DONE\n");
        std::fflush(stdout);

        char line[64];
        while (std::fgets(line, static_cast<int>(sizeof(line)), stdin) != nullptr)
        {
            if (std::strncmp(line, "perf", 4) == 0)
            {
                const uint32_t tc = BenchmarkTorque(torque, dwt);
                const uint32_t sc = BenchmarkSpeed(speed, dwt);
                const uint32_t pc = BenchmarkPosition(position, dwt);
                std::printf("DWT torque_calculate=%lu\n", static_cast<unsigned long>(tc));
                std::printf("DWT speed_calculate=%lu\n", static_cast<unsigned long>(sc));
                std::printf("DWT position_calculate=%lu\n", static_cast<unsigned long>(pc));
                std::printf("[[CYCLES]] torque_calculate=%lu speed_calculate=%lu position_calculate=%lu\n",
                    static_cast<unsigned long>(tc),
                    static_cast<unsigned long>(sc),
                    static_cast<unsigned long>(pc));
                std::printf("DONE\n");
                std::fflush(stdout);
            }
            else if (std::strncmp(line, "quit", 4) == 0)
                break;
        }
    }
}

int main(int argc, char** argv)
{
    const bool benchmarkMode = (argc >= 2 && std::strcmp(argv[1], "--benchmark") == 0);

    if (benchmarkMode)
    {
        RunDwtBenchmark();
        return 0;
    }

    sil::SilQemuApplication app;
    app.Run();
    return 0;
}
