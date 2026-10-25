#pragma once

#include "core/can/FocMotorMessages.hpp"
#include "tools/can_commander/logic/MotorSettings.hpp"
#include <QFormLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QString>
#include <QWidget>
#include <map>
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
        void ForgetValues();
        void SetUnavailable(const QString& reason);

    signals:
        void RefreshRequested();
        void ClearStatisticsRequested();

    private:
        QWidget* CreateGroup(const QString& title, std::span<const SettingPresentation> readings);
        QWidget* CreateActions();
        QString Format(can::FocParameter parameter, float value) const;

        QLabel* hint{ nullptr };
        QPushButton* refreshButton{ nullptr };
        QPushButton* clearStatisticsButton{ nullptr };
        std::map<can::FocParameter, QLabel*> values;
        QLocale numberLocale;
    };
}
