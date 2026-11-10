#include "tools/virtual_motor/app/gui/ManagerWindow.hpp"
#include "tools/virtual_motor/app/gui/AddMotorDialog.hpp"
#include "tools/virtual_motor/app/gui/MotorWindow.hpp"
#include "tools/virtual_motor/app/logic/MotorText.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QVBoxLayout>

namespace virtual_motor::app
{
    namespace
    {
        enum Column
        {
            nodeColumn,
            plantColumn,
            stateColumn,
            realTimeColumn,
            disturbancesColumn,
            ledsColumn,
            columnCount
        };

        QTableWidgetItem* Cell(const QString& text, const QString& tip = {})
        {
            auto* item = new QTableWidgetItem(text);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            item->setToolTip(tip);
            return item;
        }

        QString CrashedTip()
        {
            return "The process ended unexpectedly; the CAN Commander reports this node offline. Open the motor to read its last log lines.";
        }
    }

    ManagerWindow::ManagerWindow(Rig& rig, const QString& hubStatus, const QString& commanderProgram, QWidget* parent)
        : QMainWindow(parent)
        , rig(rig)
        , commanderProgram(commanderProgram)
        , table(new QTableWidget(0, columnCount, this))
    {
        setWindowTitle("e-foc Virtual Motor");
        table->setHorizontalHeaderLabels({ "Node", "Plant", "State", "Real time", "Disturbances", "LEDs" });
        table->horizontalHeader()->setStretchLastSection(true);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->verticalHeader()->hide();
        table->setAccessibleName("Virtual motors");

        auto* central = new QWidget(this);
        auto* layout = new QVBoxLayout(central);
        layout->addWidget(BuildHeader(hubStatus));
        layout->addWidget(table, 1);
        layout->addWidget(BuildButtons());
        setCentralWidget(central);
        BuildMenus();
        resize(860, 420);
        restoreGeometry(QSettings{}.value("manager/geometry").toByteArray());

        connect(table, &QTableWidget::itemSelectionChanged, this, &ManagerWindow::SelectionChanged);
        connect(table, &QTableWidget::cellDoubleClicked, this, [this](int row)
            {
                OpenMotor(static_cast<uint8_t>(table->item(row, nodeColumn)->text().toUInt()));
            });
        MotorsChanged();
    }

    QWidget* ManagerWindow::BuildHeader(const QString& hubStatus)
    {
        auto* header = new QWidget(this);
        auto* layout = new QHBoxLayout(header);
        auto* bus = new QLabel("Bus: default · " + hubStatus, header);
        auto* commander = new QPushButton("Open CAN Commander", header);
        commander->setEnabled(!commanderProgram.isEmpty());
        commander->setToolTip(commanderProgram.isEmpty() ? "The e-foc CAN Commander is not installed next to this app" : "Start the e-foc CAN Commander; its Interface box lists virtual:default");
        layout->addWidget(bus, 1);
        layout->addWidget(commander);
        connect(commander, &QPushButton::clicked, this, &ManagerWindow::OpenCommander);
        return header;
    }

    QWidget* ManagerWindow::BuildButtons()
    {
        auto* buttons = new QWidget(this);
        auto* layout = new QHBoxLayout(buttons);
        add = new QPushButton("Add Motor…", buttons);
        ui::backend::qt::StyleButton(*add, ui::theme::ButtonRole::Primary);
        open = new QPushButton("Open", buttons);
        powerOn = new QPushButton("Power On", buttons);
        powerOff = new QPushButton("Power Off", buttons);
        powerCycle = new QPushButton("Power Cycle", buttons);
        pause = new QPushButton("Pause", buttons);
        clearDisturbances = new QPushButton("Clear Disturbances", buttons);
        remove = new QPushButton("Remove", buttons);
        for (auto* button : { add, open, powerOn, powerOff, powerCycle, pause, clearDisturbances, remove })
            layout->addWidget(button);
        layout->addStretch(1);

        ConnectButtons();
        return buttons;
    }

    void ManagerWindow::ConnectButtons()
    {
        connect(add, &QPushButton::clicked, this, &ManagerWindow::AddMotor);
        connect(open, &QPushButton::clicked, this, [this]()
            {
                if (auto node = SelectedNode())
                    OpenMotor(*node);
            });
        connect(powerOn, &QPushButton::clicked, this, [this]()
            {
                if (auto node = SelectedNode())
                    this->rig.PowerOn(*node);
            });
        connect(powerOff, &QPushButton::clicked, this, [this]()
            {
                if (auto node = SelectedNode())
                    this->rig.PowerOff(*node);
            });
        connect(powerCycle, &QPushButton::clicked, this, [this]()
            {
                if (auto node = SelectedNode())
                    this->rig.PowerCycle(*node);
            });
        connect(pause, &QPushButton::clicked, this, [this]()
            {
                if (auto node = SelectedNode(); node && this->rig.Find(*node)->State() == MotorState::paused)
                    this->rig.Find(*node)->Send(port::Resume{});
                else if (node)
                    this->rig.Find(*node)->Send(port::Pause{});
            });
        connect(clearDisturbances, &QPushButton::clicked, this, [this]()
            {
                if (auto node = SelectedNode())
                    this->rig.Find(*node)->Send(port::ClearAll{});
            });
        connect(remove, &QPushButton::clicked, this, [this]()
            {
                if (auto node = SelectedNode())
                    this->rig.Remove(*node);
            });
    }

    void ManagerWindow::BuildMenus()
    {
        auto* motors = menuBar()->addMenu("&Motors");
        motors->addAction("&Add Motor…", QKeySequence::New, this, &ManagerWindow::AddMotor);
        motors->addSeparator();
        motors->addAction("&Quit", QKeySequence::Quit, this, &QWidget::close);
    }

    void ManagerWindow::MotorsChanged()
    {
        const auto selected = SelectedNode();
        table->setRowCount(static_cast<int>(rig.Motors().size()));
        for (int row = 0; row != table->rowCount(); ++row)
        {
            FillRow(row, *rig.Motors()[static_cast<std::size_t>(row)]);
            if (selected && rig.Motors()[static_cast<std::size_t>(row)]->Spec().node == *selected)
                table->selectRow(row);
        }
        SelectionChanged();
    }

    void ManagerWindow::MotorUpdated(uint8_t node)
    {
        for (int row = 0; row != table->rowCount(); ++row)
            if (rig.Motors()[static_cast<std::size_t>(row)]->Spec().node == node)
                FillRow(row, *rig.Motors()[static_cast<std::size_t>(row)]);

        if (auto window = windows.find(node); window != windows.end() && window->second)
            window->second->Refresh();
        SelectionChanged();
    }

    void ManagerWindow::MotorLogged(uint8_t node, const std::string& line)
    {
        if (auto window = windows.find(node); window != windows.end() && window->second)
            window->second->AppendLog(QString::fromStdString(line));
    }

    void ManagerWindow::MotorRefused(uint8_t node, const port::Refused& refused)
    {
        if (auto window = windows.find(node); window != windows.end() && window->second)
            window->second->ShowRefusal(refused);
    }

    void ManagerWindow::FillRow(int row, const RigMotor& motor)
    {
        const auto& status = motor.Status();
        const bool live = status && (motor.State() == MotorState::running || motor.State() == MotorState::paused);
        const auto pacing = live ? RealTimeText(*status) : RealTime{ "—", false, "" };
        table->setItem(row, nodeColumn, Cell(QString::number(motor.Spec().node)));
        table->setItem(row, plantColumn, Cell(QString::fromStdString(motor.Spec().plant)));
        table->setItem(row, stateColumn, Cell(QString::fromStdString(StateText(motor.State())), motor.State() == MotorState::crashed ? CrashedTip() : QString{}));
        table->setItem(row, realTimeColumn, Cell(QString::fromStdString(pacing.text), QString::fromStdString(pacing.detail)));
        table->setItem(row, disturbancesColumn, Cell(motor.ActiveDisturbances() == 0 ? QString("none") : QString("%1 active").arg(motor.ActiveDisturbances())));
        table->setItem(row, ledsColumn, Cell(live ? QString::fromStdString(LedText(status->leds)) : QString("—")));
    }

    std::optional<uint8_t> ManagerWindow::SelectedNode() const
    {
        const auto rows = table->selectionModel()->selectedRows();
        if (rows.isEmpty())
            return std::nullopt;
        return static_cast<uint8_t>(table->item(rows.front().row(), nodeColumn)->text().toUInt());
    }

    void ManagerWindow::SelectionChanged()
    {
        const auto node = SelectedNode();
        const auto* motor = node ? rig.Find(*node) : nullptr;
        const auto state = motor ? motor->State() : MotorState::stopped;
        const bool live = motor && (state == MotorState::running || state == MotorState::paused);
        const bool powered = motor && state != MotorState::stopped && state != MotorState::crashed;

        open->setEnabled(motor != nullptr);
        powerOn->setEnabled(motor && !powered);
        powerOff->setEnabled(powered);
        powerCycle->setEnabled(motor != nullptr);
        pause->setEnabled(live);
        pause->setText(state == MotorState::paused ? "Resume" : "Pause");
        clearDisturbances->setEnabled(live && motor->ActiveDisturbances() != 0);
        remove->setEnabled(motor != nullptr);
        for (auto* button : { open, powerOn, powerOff, powerCycle, pause, clearDisturbances, remove })
            button->setToolTip(motor ? QString{} : "Select a motor first");
        if (motor && !live)
            pause->setToolTip("Available while the motor runs");
    }

    void ManagerWindow::AddMotor()
    {
        std::set<uint8_t> taken;
        for (const auto& motor : rig.Motors())
            taken.insert(motor->Spec().node);

        AddMotorDialog dialog{ taken, this };
        uint8_t free{ 1 };
        while (taken.contains(free) && free != 255)
            ++free;
        dialog.SetNode(free);
        if (dialog.exec() != QDialog::Accepted)
            return;

        if (const auto refusal = rig.Add(dialog.Spec()))
            QMessageBox::warning(this, "Add Virtual Motor", QString::fromStdString(*refusal));
    }

    void ManagerWindow::OpenMotor(uint8_t node)
    {
        auto& window = windows[node];
        if (!window)
        {
            window = new MotorWindow(rig, node, nullptr);
            window->setAttribute(Qt::WA_DeleteOnClose);
        }
        window->show();
        window->raise();
        window->activateWindow();
    }

    QTableWidget& ManagerWindow::Table()
    {
        return *table;
    }

    void ManagerWindow::OpenCommander()
    {
        if (!QProcess::startDetached(commanderProgram, {}))
            QMessageBox::warning(this, "Open CAN Commander", "The e-foc CAN Commander could not be started from " + commanderProgram + ".");
    }

    void ManagerWindow::closeEvent(QCloseEvent* event)
    {
        QStringList running;
        for (const auto& motor : rig.Motors())
            if (motor->State() != MotorState::stopped && motor->State() != MotorState::crashed)
                running << QString("node %1").arg(motor->Spec().node);

        if (!running.isEmpty() &&
            QMessageBox::question(this, "Close e-foc Virtual Motor", QString("Closing powers off %1. The CAN Commander will see them go offline.").arg(running.join(", ")), QMessageBox::Close | QMessageBox::Cancel) != QMessageBox::Close)
        {
            event->ignore();
            return;
        }

        for (auto& [node, window] : windows)
            if (window)
                window->close();
        rig.PowerOffAll();
        QSettings{}.setValue("manager/geometry", saveGeometry());
        event->accept();
    }
}
