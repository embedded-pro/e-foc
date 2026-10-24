#include "tools/can_commander/gui/MotorOverview.hpp"
#include "ui/backend/qt/QtConversions.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include <QPainter>
#include <QPixmap>

namespace tool
{
    namespace
    {
        ui::theme::ColorRole ColourFor(ui::theme::StatusLevel level)
        {
            switch (level)
            {
                case ui::theme::StatusLevel::Ok:
                    return ui::theme::ColorRole::Ok;
                case ui::theme::StatusLevel::Warning:
                    return ui::theme::ColorRole::Warning;
                case ui::theme::StatusLevel::Fault:
                    return ui::theme::ColorRole::Fault;
                default:
                    return ui::theme::ColorRole::Neutral;
            }
        }
    }

    QIcon StatusIcon(ui::theme::StatusLevel level)
    {
        QPixmap pixmap{ 16, 16 };
        pixmap.fill(Qt::transparent);

        QPainter painter{ &pixmap };
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(ui::backend::qt::ToQt(ui::theme::Current().Get(ColourFor(level))));
        painter.drawEllipse(QRectF{ 3.0, 3.0, 10.0, 10.0 });
        return QIcon{ pixmap };
    }

    MotorOverview::MotorOverview(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(new QLabel("Motors:"));

        chipLayout = new QHBoxLayout();
        layout->addLayout(chipLayout);

        emptyLabel = new QLabel("none yet");
        ui::backend::qt::StyleStatusLabel(*emptyLabel, ui::theme::StatusLevel::Neutral);
        layout->addWidget(emptyLabel);
        layout->addStretch();
    }

    void MotorOverview::SetMotor(uint16_t nodeId, const QString& title, const QString& summary, ui::theme::StatusLevel level)
    {
        auto chip = chips.find(nodeId);
        if (chip == chips.end())
            chip = chips.emplace(nodeId, CreateChip(nodeId)).first;

        chip->second->setText(QString("%1 · %2").arg(title, summary));
        chip->second->setIcon(StatusIcon(level));
        chip->second->setToolTip(QString("%1: %2. Click to open its tab.").arg(title, summary));
        chip->second->setAccessibleName(QString("%1, %2").arg(title, summary));
        emptyLabel->setVisible(chips.empty());
    }

    void MotorOverview::RemoveMotor(uint16_t nodeId)
    {
        const auto chip = chips.find(nodeId);
        if (chip == chips.end())
            return;

        delete chip->second;
        chips.erase(chip);
        emptyLabel->setVisible(chips.empty());
    }

    void MotorOverview::SetCurrent(uint16_t nodeId)
    {
        for (auto& [id, chip] : chips)
            chip->setChecked(id == nodeId);
    }

    QToolButton* MotorOverview::CreateChip(uint16_t nodeId)
    {
        auto* chip = new QToolButton();
        chip->setCheckable(true);
        chip->setAutoRaise(true);
        chip->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        connect(chip, &QToolButton::clicked, this, [this, nodeId]()
            {
                emit MotorSelected(nodeId);
            });
        InsertInOrder(nodeId, chip);
        return chip;
    }

    void MotorOverview::InsertInOrder(uint16_t nodeId, QToolButton* chip)
    {
        int index{ 0 };
        for (const auto& [id, existing] : chips)
            if (id < nodeId)
                ++index;

        chipLayout->insertWidget(index, chip);
    }
}
