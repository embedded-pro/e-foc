#include "tools/can_commander/gui/TelemetryPanel.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include "ui/theme/Theme.hpp"
#include <QFormLayout>
#include <QGroupBox>
#include <QVBoxLayout>

namespace tool
{
    using namespace services;

    namespace
    {
        using ui::model::FieldId;
        using ui::model::FieldKind;
        using ui::model::FieldSpec;

        constexpr FieldId idCurrent{ 1 };
        constexpr FieldId iqCurrent{ 2 };
        constexpr FieldId speed{ 3 };
        constexpr FieldId position{ 4 };
        constexpr FieldId busVoltage{ 5 };

        constexpr FieldSpec ReadOut(FieldId id, std::string_view label, std::string_view suffix, std::uint8_t decimals)
        {
            return FieldSpec{ id, ui::model::noGroup, FieldKind::ReadOut, label, suffix, { 0.0, 0.0, 0.0, 0.0, decimals }, {}, {}, {} };
        }
    }

    TelemetryPanel::TelemetryPanel(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);

        auto* statusGroup = new QGroupBox("Motor Status");
        auto* statusLayout = new QFormLayout(statusGroup);
        motorStateLabel = new QLabel("---");
        faultLabel = new QLabel("---");
        statusLayout->addRow("State:", motorStateLabel);
        statusLayout->addRow("Fault:", faultLabel);
        layout->addWidget(statusGroup);

        measurementFields = {
            ReadOut(idCurrent, "Id Current:", " A", 3),
            ReadOut(iqCurrent, "Iq Current:", " A", 3),
            ReadOut(speed, "Speed:", " rad/s", 3),
            ReadOut(position, "Position:", " rad", 4),
            ReadOut(busVoltage, "Bus Voltage:", " V", 2)
        };

        measurementSpec = ui::model::FormSpec{ {}, measurementFields, {}, {} };
        measurementModel.emplace(measurementSpec, measurementValues, std::span<ui::model::TableModel>{});

        auto* measureGroup = new QGroupBox("Measurements");
        auto* measureLayout = new QVBoxLayout(measureGroup);
        measurementForm = new ui::backend::qt::QtFormView{ measureGroup };
        measurementForm->Build(*measurementModel);
        measureLayout->addWidget(measurementForm);
        layout->addWidget(measureGroup);

        layout->addStretch();
    }

    TelemetryPanel::~TelemetryPanel()
    {
        // ~QtFormView resets the callbacks it installed on its FormModel, and that model is a
        // member which unwinds before ~QWidget deletes its children.
        delete measurementForm;
    }

    void TelemetryPanel::Show(FieldId field, double value)
    {
        measurementModel->SetNumber(field, value);
        measurementForm->Refresh(field);
    }

    void TelemetryPanel::OnMotorStatus(FocMotorState state, FocFaultCode fault)
    {
        motorStateLabel->setText(MotorStateName(state));
        faultLabel->setText(FaultCodeName(fault));
    }

    void TelemetryPanel::OnCurrentMeasurement(float idCurrentValue, float iqCurrentValue)
    {
        Show(idCurrent, static_cast<double>(idCurrentValue));
        Show(iqCurrent, static_cast<double>(iqCurrentValue));
    }

    void TelemetryPanel::OnSpeedPosition(float speedValue, float positionValue)
    {
        Show(speed, static_cast<double>(speedValue));
        Show(position, static_cast<double>(positionValue));
    }

    void TelemetryPanel::OnBusVoltage(float voltage)
    {
        Show(busVoltage, static_cast<double>(voltage));
    }

    void TelemetryPanel::OnFaultEvent(FocFaultCode fault)
    {
        faultLabel->setText(FaultCodeName(fault));
        ui::backend::qt::StyleStatusLabel(*faultLabel, ui::theme::StatusLevel::Fault);
    }

    QString TelemetryPanel::MotorStateName(FocMotorState state)
    {
        switch (state)
        {
            case FocMotorState::idle:
                return "Idle";
            case FocMotorState::running:
                return "Running";
            case FocMotorState::fault:
                return "Fault";
            case FocMotorState::calibrating:
                return "Calibrating";
            case FocMotorState::partialCalibration:
                return "Partially calibrated";
            default:
                return "Unknown";
        }
    }

    QString TelemetryPanel::FaultCodeName(FocFaultCode fault)
    {
        switch (fault)
        {
            case FocFaultCode::none:
                return "None";
            case FocFaultCode::overCurrent:
                return "Over Current";
            case FocFaultCode::overVoltage:
                return "Over Voltage";
            case FocFaultCode::underVoltage:
                return "Under Voltage";
            case FocFaultCode::overTemperature:
                return "Over Temperature";
            case FocFaultCode::sensorFault:
                return "Sensor Fault";
            default:
                return "Unknown";
        }
    }
}
