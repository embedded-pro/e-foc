#pragma once

#include "core/can/FocMotorCanBridge.hpp"
#include "core/foc/model/drive/memory/NvmImageBuilder.hpp"
#include "core/platform_abstraction/DecimatingThreePhaseInverter.hpp"
#include "core/platform_abstraction/NoEncoderIndex.hpp"
#include "core/services/alignment/MotorAlignmentImpl.hpp"
#include "core/services/bus_voltage/BusVoltageMonitor.hpp"
#include "core/services/current_offsets/ConfirmedPhaseCurrentOffsets.hpp"
#include "core/services/current_offsets/PhaseCurrentOffsetMeasurement.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentificationImpl.hpp"
#include "core/services/fault_history/FaultHistory.hpp"
#include "core/services/index_search/IndexSearchImpl.hpp"
#include "core/services/inverter_compensation/InverterCompensation.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemoryImpl.hpp"
#include "core/services/standstill/RotorStandstillImpl.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "core/supervision/PeakCurrent.hpp"
#include "hal/interfaces/Eeprom.hpp"
#include "hal/interfaces/SerialCommunication.hpp"
#include "infra/stream/StdStringOutputStream.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/WithSharedAccess.hpp"
#include "integration_tests/model_in_the_loop/SimulatedDrive.hpp"
#include "integration_tests/support/response/PlantTrace.hpp"
#include "integration_tests/support/response/ResponseWindow.hpp"
#include "integration_tests/support/response/StepMetrics.hpp"
#include "motor_parameters/PlantPresets.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/NvmEepromRegion.hpp"
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace mil
{
    struct Scenario
    {
        sil::SilPlantConfig plant{ sil::NominalPlant() };
        std::string plantName{ "nominal" };
        sil::NvmImageSpec nvm{ .includeCalibration = false, .includeConfig = false };
        std::optional<std::vector<uint8_t>> nvmImage;
        bool samplingSkewCorrected{ true };

        Scenario& Plant(const std::string& preset);
        Scenario& Calibrated();
        Scenario& StoredCalibrationOffBy(const std::string& quantity, float factor);
        Scenario& BootsIn(state_machine::ControlMode mode);
        Scenario& BootsFrom(std::vector<uint8_t> image);
        Scenario& CurrentLoop(foc::CurrentAlgorithm algorithm);
        Scenario& SpeedLoop(foc::SpeedAlgorithm algorithm);
        Scenario& PositionLoop(foc::PositionAlgorithm algorithm);
        Scenario& Recorded(uint32_t sampleRateHz, uint32_t maxSamples);
        Scenario& TorqueStep(float torqueNm, uint32_t delayMs);
        Scenario& WithoutSamplingSkewCorrection();
    };

    struct ElectricalIdentification
    {
        float resistanceOhm;
        float inductanceMilliHenry;
        std::size_t polePairs;
    };

    class ModelInTheLoop
    {
    public:
        explicit ModelInTheLoop(const Scenario& scenario);
        ~ModelInTheLoop();

        void Run(std::chrono::microseconds duration);
        bool RunUntil(const std::function<bool()>& done, std::chrono::microseconds limit);

        bool Align();
        bool Calibrate();
        bool ClearCalibration();
        std::optional<ElectricalIdentification> IdentifyElectrical();
        bool Enable();
        bool Disable();
        bool Select(state_machine::ControlMode mode);
        bool SetTorque(float ampere);
        bool SetSpeed(float radiansPerSecond);
        bool SetPosition(float radians);

        bool CaptureAfterEnable(float milliseconds);
        bool CaptureAfterSetpoint(float milliseconds);
        bool AlternateSpeed(float low, float high, std::chrono::milliseconds dwell, std::chrono::milliseconds total);
        bool HoldSpeed(float speed, std::chrono::milliseconds duration);

        void ObserveEachMillisecond(std::function<void()> observer);

        bool IsRunning() const;
        bool IsReady() const;
        bool HasFault() const;
        state_machine::ControlModeStateMachine& ControlMode();
        foc::CurrentStream& CurrentStream();
        std::optional<state_machine::OnlineEstimates> OnlineEstimates();
        const services::CalibrationData& CalibrationRecord() const;

        const Scenario& Setup() const;
        std::vector<uint8_t> NvmImage() const;
        SimulatedDrive& Drive();
        const integration::response::PlantTrace& Trace();
        std::optional<uint32_t> SetpointTick() const;
        std::optional<float> Setpoint() const;
        uint32_t TicksFromMilliseconds(float milliseconds) const;
        std::string TracerTail() const;

    private:
        class DeferredLowPriorityInterrupt
            : public foc::LowPriorityInterrupt
        {
        public:
            void Trigger() override;
            void Register(const infra::Function<void()>& handler) override;
            void Unregister() override;

            void RunPending();

        private:
            infra::Function<void()> handler;
            bool pending{ false };
        };

        class MemoryEeprom
            : public hal::Eeprom
        {
        public:
            explicit MemoryEeprom(std::vector<uint8_t> contents);

            uint32_t Size() const override;
            void WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
            void ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
            void Erase(infra::Function<void()> onDone) override;
            const std::vector<uint8_t>& Contents() const;

        private:
            std::vector<uint8_t> contents;
        };

        void Step();
        bool Command(const std::function<void(const infra::Function<void(state_machine::CommandResult)>&)>& issue, std::chrono::microseconds limit);
        bool RunUntilState(const std::function<bool(const state_machine::State&)>& predicate, std::chrono::microseconds limit);
        void StampSetpoint(float value);
        void DrainRecords();

        static constexpr uint32_t calibrationRegionSize = 128;
        static constexpr uint32_t configRegionSize = 224;
        static constexpr uint32_t learnedModelRegionSize = 32;
        static constexpr uint32_t faultHistoryRegionSize = 128;
        static constexpr uint32_t pwmDeadTimeNs = 500;
        static constexpr float motorFluxLinkageWb = 0.007f;
        static constexpr std::size_t recordsPerDrain = 16;

        const Scenario scenario;
        infra::ClockFixture clock;
        infra::StdStringOutputStream::WithStorage tracerStream;
        services::TracerToStream tracer{ tracerStream };
        SimulatedDrive drive;
        application::DecimatingThreePhaseInverter inverter;
        DeferredLowPriorityInterrupt lowPriorityInterrupt;
        MemoryEeprom eeprom;
        foc::Volts vdc;
        const std::chrono::nanoseconds tickPeriod;
        const uint32_t ticksPerMillisecond;
        services::NvmEepromRegion calibrationRegion{ eeprom, sil::calibrationRegionOffset, calibrationRegionSize };
        services::NvmEepromRegion configRegion{ eeprom, sil::configRegionOffset, configRegionSize };
        services::NvmEepromRegion learnedModelRegion{ eeprom, sil::learnedModelRegionOffset, learnedModelRegionSize };
        services::NvmEepromRegion faultHistoryRegion{ eeprom, sil::faultHistoryRegionOffset, faultHistoryRegionSize };
        services::NonVolatileMemoryImpl nvm{ calibrationRegion, configRegion, faultHistoryRegion, learnedModelRegion };
        services::InverterCompensation inverterCompensation;
        infra::WithSharedAccess<services::PhaseCurrentOffsetMeasurement> currentOffsets;
        services::ConfirmedPhaseCurrentOffsets confirmedCurrentOffsets;
        infra::WithSharedAccess<services::ElectricalParametersIdentificationImpl> electricalIdent;
        infra::WithSharedAccess<services::MotorAlignmentImpl> motorAlignment;
        application::NoEncoderIndex noEncoderIndex;
        infra::WithSharedAccess<services::IndexSearchImpl> indexSearch;
        services::RotorStandstillImpl rotorStandstill;
        state_machine::NoOpFaultNotifier faultNotifier;
        services::ConfigData configData;
        foc::CurrentStream currentStream;
        std::optional<state_machine::ControlModeStateMachine> controlMode;
        std::optional<services::FaultHistory> faultHistory;
        supervision::PeakCurrent peakCurrent;
        foc::Volts filteredBusVoltage{ 0.0f };
        services::BusVoltageMonitor busVoltage{ [this]()
            {
                return drive.PowerSupplyVoltage();
            },
            [this](foc::Volts measured)
            {
                filteredBusVoltage = measured;
                controlMode->Commands().TrackBusVoltage(measured);
            } };

        std::vector<std::string> recordLines;
        integration::response::PlantTrace trace;
        std::optional<uint32_t> setpointTick;
        std::optional<float> setpoint;
        std::function<void()> millisecondObserver;
    };

    std::optional<integration::response::StepResponseMetrics> MeasureStep(ModelInTheLoop& mil, integration::response::Signal signal, float bandPercent, const char* label);
    std::optional<integration::response::DisturbanceResponseMetrics> MeasureDisturbance(ModelInTheLoop& mil, integration::response::Signal signal, float absoluteBand);
}
