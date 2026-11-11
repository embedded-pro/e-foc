#include "tools/can_commander/gui/DiagnosticsTab.hpp"
#include "tools/can_commander/gui/CommandPanel.hpp"
#include "tools/can_commander/gui/TelemetryPanel.hpp"
#include "tools/can_commander/logic/FaultHistoryText.hpp"
#include "ui/backend/qt/QtConversions.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include "ui/theme/Theme.hpp"
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QVBoxLayout>
#include <cmath>

namespace tool
{
    namespace
    {
        using ui::backend::qt::ToQt;

        constexpr const char* placeholder = "—";
        constexpr const char* refreshHint = "Read every value and the fault history from the controller again";
        constexpr const char* clearHint = "Restart the control-loop and CAN error statistics from zero";
        constexpr const char* clearFaultHistoryHint = "Delete every fault the controller has recorded";
        constexpr const char* recalibrateHint = "Redesign the current and speed loops now from the settled online estimates that pass their checks";
        constexpr const char* resetModelHint = "Forget the learned inductance, inertia and friction and design the loops from the calibration record again";
        constexpr const char* recalibrationText = "The model the loops are designed from: the calibration record's until settled online estimates replace it while the "
                                                  "motor is stopped. The inductance, inertia and friction learned are stored; the resistance never is.";
        constexpr const char* faultHistoryText = "The controller's last eight faults, newest first. They are kept across power cycles until cleared.";
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
        layout->addWidget(CreateGroup("Online Estimates", readings.subspan(4, 5)));
        layout->addWidget(CreateGroup("Control Loop", readings.subspan(9, 8)));
        layout->addWidget(CreateGroup("Platform", readings.subspan(17, 1)));
        layout->addWidget(CreateRecalibration());
        layout->addWidget(CreateFaultHistory());
        layout->addWidget(CreateActions());
        layout->addStretch();

        SetUnavailable({});
        SetFaultHistoryUnavailable({});
        SetRecalibrationUnavailable({}, {});
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

    QWidget* DiagnosticsTab::CreateRecalibration()
    {
        auto* group = CreateGroup("Online Recalibration", RecalibrationReadings());
        auto* form = static_cast<QFormLayout*>(group->layout());

        recalibrationHint = new QLabel();
        recalibrationHint->setWordWrap(true);
        recalibrationHint->setAccessibleName("Online recalibration status");
        ui::backend::qt::StyleStatusLabel(*recalibrationHint, ui::theme::StatusLevel::Neutral);
        form->insertRow(0, recalibrationHint);

        recalibrateButton = new QPushButton("Re&calibrate");
        resetModelButton = new QPushButton("Reset &Model…");
        auto* actions = new QHBoxLayout();
        actions->addWidget(recalibrateButton);
        actions->addStretch();
        actions->addWidget(resetModelButton);
        form->addRow(actions);

        connect(recalibrateButton, &QPushButton::clicked, this, &DiagnosticsTab::RecalibrateRequested);
        connect(resetModelButton, &QPushButton::clicked, this, &DiagnosticsTab::ResetModelRequested);
        return group;
    }

    QWidget* DiagnosticsTab::CreateFaultHistory()
    {
        auto* group = new QGroupBox("Fault History");
        auto* layout = new QVBoxLayout(group);

        faultHistoryHint = new QLabel();
        faultHistoryHint->setWordWrap(true);
        ui::backend::qt::StyleStatusLabel(*faultHistoryHint, ui::theme::StatusLevel::Neutral);
        layout->addWidget(faultHistoryHint);

        faultTable = new QTableWidget(0, 6);
        faultTable->setHorizontalHeaderLabels({ "Fault", "Mode", "Power-Up", "Uptime", "Bus Voltage", "Peak Current" });
        faultTable->horizontalHeaderItem(2)->setToolTip("The power-up of the controller the fault happened in, counted since the history was first written");
        faultTable->horizontalHeaderItem(3)->setToolTip("Time from that power-up to the fault (hours:minutes:seconds)");
        faultTable->horizontalHeaderItem(4)->setToolTip("Filtered bus voltage when the fault was entered");
        faultTable->horizontalHeaderItem(5)->setToolTip("Largest torque current over the 10 ms before the fault");
        faultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        faultTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        faultTable->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        faultTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
        faultTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        faultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        faultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        faultTable->setAccessibleName("Fault history");
        layout->addWidget(faultTable);

        faultHistoryEmpty = new QLabel(placeholder);
        faultHistoryEmpty->setAccessibleName("Fault history status");
        layout->addWidget(faultHistoryEmpty);

        auto* actions = new QHBoxLayout();
        clearFaultHistoryButton = new QPushButton("Clear Fault &History");
        actions->addWidget(clearFaultHistoryButton);
        actions->addStretch();
        layout->addLayout(actions);

        connect(clearFaultHistoryButton, &QPushButton::clicked, this, &DiagnosticsTab::ClearFaultHistoryRequested);
        RenderFaultHistory();
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

    void DiagnosticsTab::ShowFaultRecord(const can::FocFaultRecord& record)
    {
        if (record.age >= faultRecords.size())
            return;

        faultRecords[record.age] = record;
        RenderFaultHistory();
    }

    void DiagnosticsTab::ForgetValues()
    {
        for (const auto& entry : values)
            entry.second->setText(placeholder);
        faultRecords = {};
        RenderFaultHistory();
    }

    void DiagnosticsTab::RenderFaultHistory()
    {
        std::size_t rows{ 0 };
        while (rows != faultRecords.size() && faultRecords[rows].has_value() && faultRecords[rows]->code != can::FocFaultCode::none)
            ++rows;

        faultTable->setRowCount(static_cast<int>(rows));
        for (std::size_t row = 0; row != rows; ++row)
            ShowFaultRow(static_cast<int>(row), *faultRecords[row]);

        const bool read = faultRecords[0].has_value();
        faultTable->setVisible(rows != 0);
        faultHistoryEmpty->setVisible(rows == 0);
        faultHistoryEmpty->setText(read ? QString("No faults recorded.") : QString(placeholder));
    }

    void DiagnosticsTab::ShowFaultRow(int row, const can::FocFaultRecord& record)
    {
        const auto cell = [this, row](int column, const QString& text)
        {
            auto* item = new QTableWidgetItem(text);
            if (column >= 2)
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            faultTable->setItem(row, column, item);
        };

        cell(0, TelemetryPanel::FaultCodeName(record.code));
        cell(1, CommandPanel::ControlModeName(record.mode));
        cell(2, numberLocale.toString(record.powerUp));
        cell(3, ToQt(UptimeText(record.uptimeMs)));
        cell(4, numberLocale.toString(static_cast<double>(record.busVoltage.Value()), 'f', 2) + " V");
        cell(5, numberLocale.toString(static_cast<double>(record.peakCurrent.Value()), 'f', 2) + " A");
        faultTable->setVerticalHeaderItem(row, new QTableWidgetItem(row == 0 ? QString("Newest") : QString::number(row + 1)));
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

    void DiagnosticsTab::SetFaultHistoryUnavailable(const QString& reason)
    {
        const bool available = reason.isEmpty();
        faultHistoryHint->setText(available ? QString(faultHistoryText) : reason);
        clearFaultHistoryButton->setEnabled(available);
        clearFaultHistoryButton->setToolTip(available ? QString(clearFaultHistoryHint) : reason);
    }

    void DiagnosticsTab::SetRecalibrationUnavailable(const QString& recalibrateReason, const QString& resetModelReason)
    {
        recalibrationHint->setText(recalibrateReason.isEmpty() ? QString(recalibrationText) : recalibrateReason);
        recalibrateButton->setEnabled(recalibrateReason.isEmpty());
        recalibrateButton->setToolTip(recalibrateReason.isEmpty() ? QString(recalibrateHint) : recalibrateReason);
        resetModelButton->setEnabled(resetModelReason.isEmpty());
        resetModelButton->setToolTip(resetModelReason.isEmpty() ? QString(resetModelHint) : resetModelReason);
    }

    QString DiagnosticsTab::Format(can::FocParameter parameter, float value) const
    {
        const auto& presentation = PresentationOf(parameter);
        if (parameter == can::FocParameter::resetCause)
            return ToQt(ResetCauseName(value));
        if (parameter == can::FocParameter::boardKind)
            return ToQt(BoardKindName(value));
        if (IsInteger(parameter))
            return numberLocale.toString(static_cast<qlonglong>(std::llround(value))) + ToQt(presentation.suffix);
        return numberLocale.toString(static_cast<double>(value), 'g', 6) + ToQt(presentation.suffix);
    }
}
