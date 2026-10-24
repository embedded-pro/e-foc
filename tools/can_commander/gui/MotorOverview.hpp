#pragma once

#include "ui/theme/Theme.hpp"
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QToolButton>
#include <QWidget>
#include <map>

namespace tool
{
    QIcon StatusIcon(ui::theme::StatusLevel level);

    // Every motor at a glance, whichever tab is open: a fault on one motor stays visible while
    // another is being commanded.
    class MotorOverview
        : public QWidget
    {
        Q_OBJECT

    public:
        explicit MotorOverview(QWidget* parent = nullptr);

        void SetMotor(uint16_t nodeId, const QString& title, const QString& summary, ui::theme::StatusLevel level);
        void RemoveMotor(uint16_t nodeId);
        void SetCurrent(uint16_t nodeId);

    signals:
        void MotorSelected(uint16_t nodeId);

    private:
        QToolButton* CreateChip(uint16_t nodeId);
        void InsertInOrder(uint16_t nodeId, QToolButton* chip);

        QHBoxLayout* chipLayout;
        QLabel* emptyLabel;
        std::map<uint16_t, QToolButton*> chips;
    };
}
