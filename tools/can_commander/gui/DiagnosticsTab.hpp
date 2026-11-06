#pragma once

#include "core/can/FocMotorMessages.hpp"
#include "tools/can_commander/logic/MotorSettings.hpp"
#include <QFormLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QString>
#include <QTableWidget>
#include <QWidget>
#include <array>
#include <map>
#include <optional>
#include <span>

namespace tool
{
    class DiagnosticsTab
        : public QWidget
    {
        Q_OBJECT

    public:
        explicit DiagnosticsTab(QWidget* parent = nullptr);

        void ShowValue(can::FocParameter parameter, can::FocParameterValue value);
        void ShowFaultRecord(const can::FocFaultRecord& record);
        void ForgetValues();
        void SetUnavailable(const QString& reason);
        void SetFaultHistoryUnavailable(const QString& reason);

    signals:
        void RefreshRequested();
        void ClearStatisticsRequested();
        void ClearFaultHistoryRequested();

    private:
        QWidget* CreateGroup(const QString& title, std::span<const SettingPresentation> readings);
        QWidget* CreateFaultHistory();
        QWidget* CreateActions();
        QString Format(can::FocParameter parameter, float value) const;
        void RenderFaultHistory();
        void ShowFaultRow(int row, const can::FocFaultRecord& record);

        QLabel* hint{ nullptr };
        QPushButton* refreshButton{ nullptr };
        QPushButton* clearStatisticsButton{ nullptr };
        std::map<can::FocParameter, QLabel*> values;
        QLocale numberLocale;

        QLabel* faultHistoryHint{ nullptr };
        QTableWidget* faultTable{ nullptr };
        QLabel* faultHistoryEmpty{ nullptr };
        QPushButton* clearFaultHistoryButton{ nullptr };
        std::array<std::optional<can::FocFaultRecord>, can::focFaultHistoryDepth> faultRecords{};
        QString faultHistoryUnavailable;
    };
}
