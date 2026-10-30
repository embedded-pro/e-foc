#include "tools/can_commander/gui/TelemetryPanel.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "tools/can_commander/gui/DisplayFormat.hpp"
#include "tools/can_commander/logic/SetpointAxis.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include "ui/theme/Theme.hpp"
#include <QFormLayout>
#include <QGroupBox>
#include <QVBoxLayout>
#include <chrono>

namespace tool
{
    using namespace services;

    namespace
    {
        using ui::model::FieldId;
        using ui::model::FieldKind;
        using ui::model::FieldSpec;
        using ui::model::ReadOutStyle;

        constexpr FieldId idCurrent{ 1 };
        constexpr FieldId iqCurrent{ 2 };
        constexpr FieldId speed{ 3 };
        constexpr FieldId position{ 4 };

        constexpr FieldId resistance{ 11 };
        constexpr FieldId inductance{ 12 };
        constexpr FieldId polePairs{ 13 };

        constexpr FieldSpec ReadOut(FieldId id, std::string_view label, std::string_view suffix, int decimals, ReadOutStyle style = ReadOutStyle::Fixed)
        {
            return FieldSpec{ id, ui::model::noGroup, FieldKind::ReadOut, label, suffix, { 0.0, 0.0, 0.0, 0.0, static_cast<std::uint8_t>(decimals), 0.0, style }, {}, {}, {} };
        }

        const std::array<FieldSpec, 4> measurementFields{
            ReadOut(idCurrent, "Id current:", " A", DecimalsFor(can::focStreamCurrentScale)),
            ReadOut(iqCurrent, "Iq current:", " A", DecimalsFor(can::focStreamCurrentScale)),
            ReadOut(speed, "Speed:", SpeedAxis().Suffix(), SpeedAxis().Decimals()),
            ReadOut(position, "Position:", PositionAxis().Suffix(), PositionAxis().Decimals())
        };

        constexpr std::chrono::milliseconds streamRefreshInterval{ 40 };

        constexpr std::array<FieldSpec, 3> parameterFields{
            ReadOut(resistance, "Resistance:", " Ω", 4, ReadOutStyle::Significant),
            ReadOut(inductance, "Inductance:", " mH", 4, ReadOutStyle::Significant),
            ReadOut(polePairs, "Pole pairs:", "", 0)
        };

        constexpr const char* placeholder{ "—" };
    }

    TelemetryPanel::TelemetryPanel(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        auto* statusGroup = new QGroupBox("Motor Status");
        auto* statusLayout = new QFormLayout(statusGroup);
        motorStateLabel = new QLabel();
        faultLabel = new QLabel();
        statusLayout->addRow("State:", motorStateLabel);
        statusLayout->addRow("Fault:", faultLabel);
        layout->addWidget(statusGroup);

        BuildForm(measurements, measurementFields, measurementValues, "Measurements");
        layout->addWidget(measurements.view->parentWidget());
        BuildForm(parameters, parameterFields, parameterValues, "Identified Parameters");
        layout->addWidget(parameters.view->parentWidget());
        layout->addStretch();

        streamRefresh.setSingleShot(true);
        streamRefresh.setInterval(streamRefreshInterval);
        connect(&streamRefresh, &QTimer::timeout, this, &TelemetryPanel::ShowLatestStreamSample);

        Clear();
    }

    TelemetryPanel::~TelemetryPanel()
    {
        // ~QtFormView resets the callbacks it installed on its FormModel, and that model is a
        // member which unwinds before ~QWidget deletes its children.
        delete measurements.view;
        delete parameters.view;
    }

    void TelemetryPanel::BuildForm(Form& form, std::span<const FieldSpec> fields, std::span<ui::model::FieldValue> values, const char* title)
    {
        form.spec = ui::model::FormSpec{ {}, fields, {}, {} };
        form.model.emplace(form.spec, values, std::span<ui::model::TableModel>{});

        auto* group = new QGroupBox(title);
        auto* groupLayout = new QVBoxLayout(group);
        form.view = new ui::backend::qt::QtFormView{ group };
        form.view->Build(*form.model);
        groupLayout->addWidget(form.view);
    }

    void TelemetryPanel::Show(Form& form, FieldId field, double value)
    {
        form.model->SetNumber(field, value);
        form.view->Refresh(field);
    }

    void TelemetryPanel::ShowPlaceholders(const Form& form)
    {
        for (const auto& field : form.spec.fields)
            if (auto* label = qobject_cast<QLabel*>(form.view->ControlFor(field.id)))
                label->setText(placeholder);
    }

    void TelemetryPanel::Clear()
    {
        streamRefresh.stop();
        latestStreamSample.reset();
        motorStateLabel->setText(placeholder);
        ui::backend::qt::StyleStatusLabel(*motorStateLabel, ui::theme::StatusLevel::Neutral);
        faultLabel->setText(placeholder);
        ui::backend::qt::StyleStatusLabel(*faultLabel, ui::theme::StatusLevel::Neutral);
        ShowPlaceholders(measurements);
        ShowPlaceholders(parameters);
    }

    void TelemetryPanel::OnMotorStatus(FocMotorState state, FocFaultCode fault)
    {
        motorStateLabel->setText(MotorStateName(state));
        const auto level = state == FocMotorState::fault     ? ui::theme::StatusLevel::Fault
                           : state == FocMotorState::running ? ui::theme::StatusLevel::Ok
                                                             : ui::theme::StatusLevel::Neutral;
        ui::backend::qt::StyleStatusLabel(*motorStateLabel, level);
        ShowFault(fault);
    }

    void TelemetryPanel::OnCurrentStream(float idCurrentValue, float iqCurrentValue, float positionValue)
    {
        latestStreamSample = StreamSample{ idCurrentValue, iqCurrentValue, positionValue };
        if (!streamRefresh.isActive())
            streamRefresh.start();
    }

    void TelemetryPanel::ShowLatestStreamSample()
    {
        if (!latestStreamSample)
            return;

        Show(measurements, idCurrent, static_cast<double>(latestStreamSample->idCurrent));
        Show(measurements, iqCurrent, static_cast<double>(latestStreamSample->iqCurrent));
        Show(measurements, position, PositionAxis().DisplayOfSi(latestStreamSample->position));
        latestStreamSample.reset();
    }

    void TelemetryPanel::OnSpeedPosition(float speedValue, float positionValue)
    {
        Show(measurements, speed, SpeedAxis().DisplayOfSi(speedValue));
        Show(measurements, position, PositionAxis().DisplayOfSi(positionValue));
    }

    void TelemetryPanel::OnFaultEvent(FocFaultCode fault)
    {
        ShowFault(fault);
    }

    void TelemetryPanel::OnElectricalParameters(float resistanceOhm, float inductanceMilliHenry, uint8_t polePairsValue)
    {
        Show(parameters, resistance, static_cast<double>(resistanceOhm));
        Show(parameters, inductance, static_cast<double>(inductanceMilliHenry));
        Show(parameters, polePairs, static_cast<double>(polePairsValue));
    }

    void TelemetryPanel::ShowFault(FocFaultCode fault)
    {
        faultLabel->setText(FaultCodeName(fault));
        ui::backend::qt::StyleStatusLabel(*faultLabel, fault == FocFaultCode::none ? ui::theme::StatusLevel::Ok : ui::theme::StatusLevel::Fault);
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
                return "Over current";
            case FocFaultCode::overVoltage:
                return "Over voltage";
            case FocFaultCode::underVoltage:
                return "Under voltage";
            case FocFaultCode::overTemperature:
                return "Over temperature";
            case FocFaultCode::sensorFault:
                return "Sensor fault";
            default:
                return "Unknown";
        }
    }
}
