#include "tools/can_commander/gui/DiagnosticsTab.hpp"
#include "ui/backend/qt/QtConversions.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include "ui/theme/Theme.hpp"
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <cmath>

namespace tool
{
    namespace
    {
        using ui::backend::qt::ToQt;

        constexpr const char* placeholder = "—";
        constexpr const char* refreshHint = "Read every value from the controller again";
        constexpr const char* clearHint = "Restart the control-loop and CAN error statistics from zero";
    }

    DiagnosticsTab::DiagnosticsTab(QWidget* parent)
        : QWidget(parent)
    {
        numberLocale.setNumberOptions(QLocale::OmitGroupSeparator);

        auto* layout = new QVBoxLayout(this);
        hint = new QLabel();
        hint->setWordWrap(true);
        ui::backend::qt::StyleStatusLabel(*hint, ui::theme::StatusLevel::Neutral);
        layout->addWidget(hint);

        const auto readings = Readings();
        layout->addWidget(CreateGroup("Calibration Record", readings.subspan(0, 4)));
        layout->addWidget(CreateGroup("Online Estimates", readings.subspan(4, 4)));
        layout->addWidget(CreateGroup("Control Loop", readings.subspan(8, 8)));
        layout->addWidget(CreateGroup("Platform", readings.subspan(16, 1)));
        layout->addWidget(CreateActions());
        layout->addStretch();

        SetUnavailable({});
    }

    QWidget* DiagnosticsTab::CreateGroup(const QString& title, std::span<const SettingPresentation> readings)
    {
        auto* group = new QGroupBox(title);
        auto* form = new QFormLayout(group);

        for (const auto& reading : readings)
        {
            auto* value = new QLabel(placeholder);
            value->setTextInteractionFlags(Qt::TextSelectableByMouse);
            value->setToolTip(ToQt(reading.description));
            value->setAccessibleName(ToQt(reading.name));

            auto* label = new QLabel(ToQt(reading.label));
            label->setToolTip(ToQt(reading.description));
            form->addRow(label, value);
            values[reading.parameter] = value;
        }

        return group;
    }

    QWidget* DiagnosticsTab::CreateActions()
    {
        auto* actions = new QWidget();
        auto* layout = new QHBoxLayout(actions);
        layout->setContentsMargins(0, 0, 0, 0);

        refreshButton = new QPushButton("&Refresh");
        clearStatisticsButton = new QPushButton("Clear &Statistics");
        layout->addWidget(refreshButton);
        layout->addWidget(clearStatisticsButton);
        layout->addStretch();

        connect(refreshButton, &QPushButton::clicked, this, &DiagnosticsTab::RefreshRequested);
        connect(clearStatisticsButton, &QPushButton::clicked, this, &DiagnosticsTab::ClearStatisticsRequested);
        return actions;
    }

    void DiagnosticsTab::ShowValue(can::FocParameter parameter, can::FocParameterValue value)
    {
        if (const auto entry = values.find(parameter); entry != values.end())
            entry->second->setText(Format(parameter, value.value));
    }

    void DiagnosticsTab::ForgetValues()
    {
        for (const auto& entry : values)
            entry.second->setText(placeholder);
    }

    void DiagnosticsTab::SetUnavailable(const QString& reason)
    {
        const bool available = reason.isEmpty();
        hint->setText(available ? QString("Values the controller measured or derived; they cannot be edited. Refresh reads them again.") : reason);
        refreshButton->setEnabled(available);
        refreshButton->setToolTip(available ? QString(refreshHint) : reason);
        clearStatisticsButton->setEnabled(available);
        clearStatisticsButton->setToolTip(available ? QString(clearHint) : reason);
    }

    QString DiagnosticsTab::Format(can::FocParameter parameter, float value) const
    {
        const auto& presentation = PresentationOf(parameter);
        if (parameter == can::FocParameter::resetCause)
            return ToQt(ResetCauseName(value));
        if (IsInteger(parameter))
            return numberLocale.toString(static_cast<qlonglong>(std::llround(value))) + ToQt(presentation.suffix);
        return numberLocale.toString(static_cast<double>(value), 'g', 6) + ToQt(presentation.suffix);
    }
}
