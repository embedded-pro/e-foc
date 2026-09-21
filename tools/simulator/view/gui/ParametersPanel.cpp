#include "tools/simulator/view/gui/ParametersPanel.hpp"
#include "tools/simulator/view/gui/QtOwned.hpp"
#include <QTabWidget>
#include <QVBoxLayout>

namespace simulator
{
    namespace
    {
        using ui::model::FieldId;
        using ui::model::FieldKind;
        using ui::model::FieldSpec;
        using ui::model::GroupId;
        using ui::model::GroupSpec;
        using ui::model::ReadOutStyle;

        constexpr GroupId electricalGroup{ 1 };
        constexpr GroupId mechanicalGroup{ 2 };
        constexpr GroupId calibrationGroup{ 3 };
        constexpr GroupId currentPidGroup{ 4 };
        constexpr GroupId speedPidGroup{ 5 };
        constexpr GroupId positionPidGroup{ 6 };

        constexpr FieldId resistance{ 1 };
        constexpr FieldId inductance{ 2 };
        constexpr FieldId friction{ 3 };
        constexpr FieldId inertia{ 4 };
        constexpr FieldId polePairs{ 5 };
        constexpr FieldId alignmentOffset{ 6 };
        constexpr FieldId currentKp{ 7 };
        constexpr FieldId currentKi{ 8 };
        constexpr FieldId speedKp{ 9 };
        constexpr FieldId speedKi{ 10 };
        constexpr FieldId speedKd{ 11 };
        constexpr FieldId positionKp{ 12 };
        constexpr FieldId positionKi{ 13 };
        constexpr FieldId positionKd{ 14 };

        constexpr GroupId noiseGroup{ 1 };
        constexpr GroupId thermalGroup{ 2 };
        constexpr GroupId liveThermalGroup{ 3 };
        constexpr GroupId encoderNoiseGroup{ 4 };

        constexpr FieldId sigma{ 1 };
        constexpr FieldId biasA{ 2 };
        constexpr FieldId biasB{ 3 };
        constexpr FieldId biasC{ 4 };
        constexpr FieldId ambient{ 5 };
        constexpr FieldId thermalResistance{ 6 };
        constexpr FieldId thermalCapacitance{ 7 };
        constexpr FieldId copperCoefficient{ 8 };
        constexpr FieldId ironCoefficient{ 9 };
        constexpr FieldId windingTemperature{ 10 };
        constexpr FieldId effectiveResistance{ 11 };
        constexpr FieldId effectiveInductance{ 12 };
        constexpr FieldId encoderSigma{ 13 };
        constexpr FieldId encoderBias{ 14 };

        constexpr ui::model::ActionId resetTemperature{ 1 };

        // The panel reports identified motor parameters, which span from 2.2e-4 kg·m² to gains in
        // the thousands. Significant digits keep both ends readable where a fixed count cannot.
        constexpr FieldSpec ReadOut(FieldId id, GroupId group, std::string_view label, std::string_view suffix, std::uint8_t digits)
        {
            return FieldSpec{ id, group, FieldKind::ReadOut, label, suffix, { 0.0, 0.0, 0.0, 0.0, digits, 0.0, ReadOutStyle::Significant }, {}, {}, {} };
        }

        constexpr FieldSpec FixedReadOut(FieldId id, GroupId group, std::string_view label, std::string_view suffix, std::uint8_t decimals)
        {
            return FieldSpec{ id, group, FieldKind::ReadOut, label, suffix, { 0.0, 0.0, 0.0, 0.0, decimals }, {}, {}, {} };
        }

        constexpr FieldSpec Number(FieldId id, GroupId group, std::string_view label, std::string_view suffix,
            double minimum, double maximum, double step, double initial, std::uint8_t decimals)
        {
            return FieldSpec{ id, group, FieldKind::Number, label, suffix, { minimum, maximum, step, initial, decimals }, {}, {}, {} };
        }
    }

    ParametersPanel::ParametersPanel(const foc::ThreePhaseMotorModel::Parameters& motorParameters, const PidParameters& pidParameters, QWidget* parent)
        : QWidget(parent)
    {
        BuildParameterSpec(pidParameters);
        BuildConfigurationSpec();

        auto* outerLayout = QtOwned<QVBoxLayout>(this);
        outerLayout->setContentsMargins(0, 0, 0, 0);

        auto* tabs = QtOwned<QTabWidget>(this);
        outerLayout->addWidget(tabs);

        auto* parametersTab = QtOwned<QWidget>(this);
        auto* parametersLayout = QtOwned<QVBoxLayout>(parametersTab);
        parameterForm = QtOwned<ui::backend::qt::QtFormView>(parametersTab);
        parameterForm->Build(*parameterModel);
        parametersLayout->addWidget(parameterForm);
        parametersLayout->addStretch();
        tabs->addTab(parametersTab, "Parameters");

        auto* configTab = QtOwned<QWidget>(this);
        auto* configLayout = QtOwned<QVBoxLayout>(configTab);
        configForm = QtOwned<ui::backend::qt::QtFormView>(configTab);
        configForm->Build(*configModel);
        configLayout->addWidget(configForm);
        configLayout->addStretch();
        tabs->addTab(configTab, "Configuration");

        // QtFormView installs its own handler in Build, so this replaces it. The configuration
        // spec declares no conditions, which is the only thing that handler applies.
        configModel->onFieldChanged = [this](FieldId changed)
        {
            if (changed == sigma || changed == biasA || changed == biasB || changed == biasC)
                EmitNoiseConfig();
            else if (changed == encoderSigma || changed == encoderBias)
                EmitEncoderNoiseConfig();
            else
                EmitThermalConfig();
        };

        configModel->onActionTriggered = [this](ui::model::ActionId)
        {
            emit thermalResetRequested();
        };

        Show(resistance, static_cast<double>(motorParameters.R.Value()));
        Show(inductance, static_cast<double>(motorParameters.Ld.Value()) * 1000.0);
        Show(friction, static_cast<double>(motorParameters.B.Value()));
        Show(inertia, static_cast<double>(motorParameters.J.Value()));
        Show(polePairs, static_cast<double>(motorParameters.p));

        UpdatePidParameters(pidParameters);
    }

    ParametersPanel::~ParametersPanel()
    {
        // ~QtFormView resets the callbacks it installed on its FormModel, and both models are
        // members that unwind before ~QWidget deletes its children.
        delete parameterForm;
        delete configForm;
    }

    void ParametersPanel::BuildParameterSpec(const PidParameters& pidParameters)
    {
        auto addGroup = [this](GroupId id, std::string_view title)
        {
            parameterGroups[parameterGroupsUsed++] = GroupSpec{ id, title, {} };
        };

        auto addField = [this](const FieldSpec& field)
        {
            parameterFields[parameterFieldsUsed++] = field;
        };

        addGroup(electricalGroup, "Electrical Parameters");
        addField(ReadOut(resistance, electricalGroup, "Resistance (R):", " \xCE\xA9", 4));
        addField(ReadOut(inductance, electricalGroup, "Inductance (L):", " mH", 4));

        addGroup(mechanicalGroup, "Mechanical Parameters");
        addField(ReadOut(friction, mechanicalGroup, "Friction (B):", " N\xC2\xB7m\xC2\xB7s/rad", 4));
        addField(ReadOut(inertia, mechanicalGroup, "Inertia (J):", " kg\xC2\xB7m\xC2\xB2", 4));

        addGroup(calibrationGroup, "Calibration Results");
        addField(ReadOut(polePairs, calibrationGroup, "Pole pairs (p):", "", 4));
        addField(ReadOut(alignmentOffset, calibrationGroup, "Alignment offset:", " rad", 4));

        addGroup(currentPidGroup, "Current Controller (PI)");
        addField(ReadOut(currentKp, currentPidGroup, "Kp:", "", 6));
        addField(ReadOut(currentKi, currentPidGroup, "Ki:", "", 6));

        if (pidParameters.speed)
        {
            addGroup(speedPidGroup, "Speed Controller (PID)");
            addField(ReadOut(speedKp, speedPidGroup, "Kp:", "", 6));
            addField(ReadOut(speedKi, speedPidGroup, "Ki:", "", 6));
            addField(ReadOut(speedKd, speedPidGroup, "Kd:", "", 6));
        }

        if (pidParameters.position)
        {
            addGroup(positionPidGroup, "Position Controller (PID)");
            addField(ReadOut(positionKp, positionPidGroup, "Kp:", "", 6));
            addField(ReadOut(positionKi, positionPidGroup, "Ki:", "", 6));
            addField(ReadOut(positionKd, positionPidGroup, "Kd:", "", 6));
        }

        parameterSpec = ui::model::FormSpec{ std::span{ parameterGroups }.first(parameterGroupsUsed),
            std::span{ parameterFields }.first(parameterFieldsUsed), {}, {} };
        parameterModel.emplace(parameterSpec, std::span{ parameterValues }.first(parameterFieldsUsed), std::span<ui::model::TableModel>{});
    }

    void ParametersPanel::BuildConfigurationSpec()
    {
        configGroups = { GroupSpec{ noiseGroup, "ADC Noise", {} }, GroupSpec{ thermalGroup, "Thermal Config", {} },
            GroupSpec{ liveThermalGroup, "Live Thermal", {} }, GroupSpec{ encoderNoiseGroup, "Encoder Noise", {} } };

        configFields = {
            Number(sigma, noiseGroup, "Sigma:", " mA", 0.0, 500.0, 1.0, 0.0, 2),
            Number(biasA, noiseGroup, "Bias A:", " mA", -500.0, 500.0, 1.0, 0.0, 2),
            Number(biasB, noiseGroup, "Bias B:", " mA", -500.0, 500.0, 1.0, 0.0, 2),
            Number(biasC, noiseGroup, "Bias C:", " mA", -500.0, 500.0, 1.0, 0.0, 2),
            Number(ambient, thermalGroup, "T_ambient (\xC2\xB0""C):", "", -40.0, 150.0, 1.0, 25.0, 2),
            Number(thermalResistance, thermalGroup, "R_th (\xC2\xB0""C/W):", "", 0.01, 100.0, 0.1, 2.0, 2),
            Number(thermalCapacitance, thermalGroup, "C_th (J/\xC2\xB0""C):", "", 0.1, 1000.0, 0.5, 25.0, 2),
            Number(copperCoefficient, thermalGroup, "\xCE\xB1_Cu (1/\xC2\xB0""C):", "", 0.0, 0.01, 0.0001, 0.00393, 5),
            Number(ironCoefficient, thermalGroup, "\xCE\xB2_Fe (1/\xC2\xB0""C):", "", -0.001, 0.001, 0.0001, 0.0, 5),
            FixedReadOut(windingTemperature, liveThermalGroup, "T_winding (\xC2\xB0""C):", " \xC2\xB0""C", 1),
            ReadOut(effectiveResistance, liveThermalGroup, "R(T) (\xCE\xA9):", " \xCE\xA9", 4),
            ReadOut(effectiveInductance, liveThermalGroup, "L_d(T) (mH):", " mH", 4),
            Number(encoderSigma, encoderNoiseGroup, "Sigma:", " mrad", 0.0, 1000.0, 0.5, 0.0, 2),
            Number(encoderBias, encoderNoiseGroup, "Bias:", " mrad", -3141.59, 3141.59, 1.0, 0.0, 2)
        };

        configActions = { ui::model::ActionSpec{ resetTemperature, "Reset Temperature", ui::theme::ButtonRole::Default, 0 } };

        configSpec = ui::model::FormSpec{ configGroups, configFields, configActions, {} };
        configModel.emplace(configSpec, configValues, std::span<ui::model::TableModel>{});
    }

    void ParametersPanel::Show(FieldId field, double value)
    {
        parameterModel->SetNumber(field, value);

        if (parameterForm != nullptr)
            parameterForm->Refresh(field);
    }

    void ParametersPanel::UpdatePidParameters(const PidParameters& pidParameters)
    {
        Show(currentKp, static_cast<double>(pidParameters.current.kp));
        Show(currentKi, static_cast<double>(pidParameters.current.ki));

        if (pidParameters.speed)
        {
            Show(speedKp, static_cast<double>(pidParameters.speed->kp));
            Show(speedKi, static_cast<double>(pidParameters.speed->ki));
            Show(speedKd, static_cast<double>(pidParameters.speed->kd));
        }

        if (pidParameters.position)
        {
            Show(positionKp, static_cast<double>(pidParameters.position->kp));
            Show(positionKi, static_cast<double>(pidParameters.position->ki));
            Show(positionKd, static_cast<double>(pidParameters.position->kd));
        }
    }

    void ParametersPanel::UpdateResistance(foc::Ohm value)
    {
        Show(resistance, static_cast<double>(value.Value()));
    }

    void ParametersPanel::UpdateInductance(foc::MilliHenry value)
    {
        Show(inductance, static_cast<double>(value.Value()));
    }

    void ParametersPanel::UpdateFriction(foc::NewtonMeterSecondPerRadian value)
    {
        Show(friction, static_cast<double>(value.Value()));
    }

    void ParametersPanel::UpdateInertia(foc::NewtonMeterSecondSquared value)
    {
        Show(inertia, static_cast<double>(value.Value()));
    }

    void ParametersPanel::UpdatePolePairs(std::size_t value)
    {
        Show(polePairs, static_cast<double>(value));
    }

    void ParametersPanel::UpdateAlignmentOffset(foc::Radians value)
    {
        Show(alignmentOffset, static_cast<double>(value.Value()));
    }

    void ParametersPanel::UpdateLiveThermal(float tempCelsius, foc::Ohm rEff, foc::Henry lEff)
    {
        configModel->SetNumber(windingTemperature, static_cast<double>(tempCelsius));
        configModel->SetNumber(effectiveResistance, static_cast<double>(rEff.Value()));
        configModel->SetNumber(effectiveInductance, static_cast<double>(lEff.Value()) * 1000.0);

        for (const auto field : { windingTemperature, effectiveResistance, effectiveInductance })
            configForm->Refresh(field);
    }

    void ParametersPanel::EmitNoiseConfig()
    {
        foc::ThreePhaseMotorModel::NoiseConfig c;
        c.sigmaAmpere = static_cast<float>(configModel->Number(sigma) / 1000.0);
        c.biasAmpereA = static_cast<float>(configModel->Number(biasA) / 1000.0);
        c.biasAmpereB = static_cast<float>(configModel->Number(biasB) / 1000.0);
        c.biasAmpereC = static_cast<float>(configModel->Number(biasC) / 1000.0);
        emit noiseConfigChanged(c);
    }

    void ParametersPanel::EmitEncoderNoiseConfig()
    {
        foc::ThreePhaseMotorModel::EncoderNoiseConfig c;
        c.sigmaRadians = static_cast<float>(configModel->Number(encoderSigma) / 1000.0);
        c.biasRadians = static_cast<float>(configModel->Number(encoderBias) / 1000.0);
        emit encoderNoiseConfigChanged(c);
    }

    void ParametersPanel::EmitThermalConfig()
    {
        foc::ThreePhaseMotorModel::ThermalConfig c;
        c.ambientCelsius = static_cast<float>(configModel->Number(ambient));
        c.thermalResistance = static_cast<float>(configModel->Number(thermalResistance));
        c.thermalCapacitance = static_cast<float>(configModel->Number(thermalCapacitance));
        c.copperTempCoeff = static_cast<float>(configModel->Number(copperCoefficient));
        c.ironInductanceCoeff = static_cast<float>(configModel->Number(ironCoefficient));
        emit thermalConfigChanged(c);
    }
}
