#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "ui/backend/qt/QtFormView.hpp"
#include "ui/model/FormModel.hpp"
#include <QWidget>
#include <array>
#include <cstddef>
#include <optional>

namespace simulator
{
    class ParametersPanel
        : public QWidget
    {
        Q_OBJECT

    public:
        struct LoopPid
        {
            float kp;
            float ki;
            float kd;
        };

        struct PidParameters
        {
            LoopPid current;
            std::optional<LoopPid> speed;
            std::optional<LoopPid> position;
        };

        ParametersPanel(const foc::ThreePhaseMotorModel::Parameters& motorParameters, const PidParameters& pidParameters, QWidget* parent = nullptr);
        ~ParametersPanel() override;

        void UpdatePidParameters(const PidParameters& pidParameters);
        void UpdateResistance(foc::Ohm value);
        void UpdateInductance(foc::MilliHenry value);
        void UpdateFriction(foc::NewtonMeterSecondPerRadian value);
        void UpdateInertia(foc::NewtonMeterSecondSquared value);
        void UpdatePolePairs(std::size_t value);
        void UpdateAlignmentOffset(foc::Radians value);
        void UpdateLiveThermal(float tempCelsius, foc::Ohm rEff, foc::Henry lEff);

    signals:
        void noiseConfigChanged(foc::ThreePhaseMotorModel::NoiseConfig config);
        void encoderNoiseConfigChanged(foc::ThreePhaseMotorModel::EncoderNoiseConfig config);
        void thermalConfigChanged(foc::ThreePhaseMotorModel::ThermalConfig config);
        void thermalResetRequested();

    private:
        static constexpr std::size_t maxParameterGroups{ 6 };
        static constexpr std::size_t maxParameterFields{ 14 };
        static constexpr std::size_t configGroupCount{ 4 };
        static constexpr std::size_t configFieldCount{ 14 };

        void BuildParameterSpec(const PidParameters& pidParameters);
        void BuildConfigurationSpec();
        void Show(ui::model::FieldId field, double value);
        void EmitNoiseConfig();
        void EmitEncoderNoiseConfig();
        void EmitThermalConfig();

        std::array<ui::model::GroupSpec, maxParameterGroups> parameterGroups{};
        std::array<ui::model::FieldSpec, maxParameterFields> parameterFields{};
        std::array<ui::model::FieldValue, maxParameterFields> parameterValues{};
        std::size_t parameterGroupsUsed{ 0 };
        std::size_t parameterFieldsUsed{ 0 };
        ui::model::FormSpec parameterSpec{};
        std::optional<ui::model::FormModel> parameterModel;

        std::array<ui::model::GroupSpec, configGroupCount> configGroups{};
        std::array<ui::model::FieldSpec, configFieldCount> configFields{};
        std::array<ui::model::FieldValue, configFieldCount> configValues{};
        std::array<ui::model::ActionSpec, 1> configActions{};
        ui::model::FormSpec configSpec{};
        std::optional<ui::model::FormModel> configModel;

        ui::backend::qt::QtFormView* parameterForm{ nullptr };
        ui::backend::qt::QtFormView* configForm{ nullptr };
    };
}
