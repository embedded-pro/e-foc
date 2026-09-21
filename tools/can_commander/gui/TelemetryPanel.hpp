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

    public slots:
        void OnMotorStatus(FocMotorState state, FocFaultCode fault);
        void OnCurrentMeasurement(float idCurrent, float iqCurrent);
        void OnSpeedPosition(float speed, float position);
        void OnBusVoltage(float voltage);
        void OnFaultEvent(FocFaultCode fault);

    private:
        static QString MotorStateName(FocMotorState state);
        static QString FaultCodeName(FocFaultCode fault);

        void Show(ui::model::FieldId field, double value);

        // The state and fault rows stay native: they read as words rather than numbers, and
        // ui::model has no text read-out by design. The fault row is also what StyleStatusLabel
        // turns red, which needs the widget itself.
        QLabel* motorStateLabel;
        QLabel* faultLabel;

        std::array<ui::model::FieldSpec, 5> measurementFields{};
        std::array<ui::model::FieldValue, 5> measurementValues{};
        ui::model::FormSpec measurementSpec{};
        std::optional<ui::model::FormModel> measurementModel;

        ui::backend::qt::QtFormView* measurementForm{ nullptr };
    };
}
