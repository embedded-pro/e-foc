#pragma once

#include "tools/can_commander/logic/CanCommandClient.hpp"
#include "ui/backend/qt/QtFormView.hpp"
#include "ui/model/FormModel.hpp"
#include <QLabel>
#include <QWidget>
#include <array>
#include <optional>

namespace tool
{
    class TelemetryPanel : public QWidget
    {
        Q_OBJECT

    public:
        explicit TelemetryPanel(QWidget* parent = nullptr);
        ~TelemetryPanel() override;

        static QString MotorStateName(FocMotorState state);
        static QString FaultCodeName(FocFaultCode fault);

    public slots:
        void OnMotorStatus(FocMotorState state, FocFaultCode fault);
        void OnCurrentMeasurement(float idCurrent, float iqCurrent);
        void OnSpeedPosition(float speed, float position);
        void OnBusVoltage(float voltage);
        void OnFaultEvent(FocFaultCode fault);
        void OnElectricalParameters(float resistanceOhm, float inductanceMilliHenry, uint8_t polePairs);
        void OnMechanicalParameters(float frictionNmSPerRad, float inertiaKgM2);

        void Clear();

    private:
        struct Form
        {
            ui::model::FormSpec spec{};
            std::optional<ui::model::FormModel> model;
            ui::backend::qt::QtFormView* view{ nullptr };
        };

        void BuildForm(Form& form, std::span<const ui::model::FieldSpec> fields, std::span<ui::model::FieldValue> values, const char* title);
        static void Show(Form& form, ui::model::FieldId field, double value);
        static void ShowPlaceholders(const Form& form);

        void ShowFault(FocFaultCode fault);

        // The state and fault rows stay native: they read as words rather than numbers, and
        // ui::model has no text read-out by design. The fault row is also what StyleStatusLabel
        // turns red, which needs the widget itself.
        QLabel* motorStateLabel;
        QLabel* faultLabel;

        std::array<ui::model::FieldValue, 5> measurementValues{};
        std::array<ui::model::FieldValue, 5> parameterValues{};
        Form measurements;
        Form parameters;
    };
}
