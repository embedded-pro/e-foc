#pragma once

#include "tools/virtual_motor/app/logic/Rig.hpp"
#include <QMainWindow>
#include <QPointer>
#include <map>
#include <optional>

class QLabel;
class QPushButton;
class QTableWidget;

namespace virtual_motor::app
{
    class MotorWindow;

    class ManagerWindow
        : public QMainWindow
        , public RigObserver
    {
        Q_OBJECT

    public:
        ManagerWindow(Rig& rig, const QString& hubStatus, const QString& commanderProgram, QWidget* parent = nullptr);

        void MotorsChanged() override;
        void MotorUpdated(uint8_t node) override;
        void MotorLogged(uint8_t node, const std::string& line) override;
        void MotorRefused(uint8_t node, const port::Refused& refused) override;
        void MotorCaptured(uint8_t node) override;

        void AddMotor();
        void OpenMotor(uint8_t node);
        QTableWidget& Table();

    protected:
        void closeEvent(QCloseEvent* event) override;

    private:
        QWidget* BuildHeader(const QString& hubStatus);
        QWidget* BuildButtons();
        void ConnectButtons();
        void BuildMenus();
        void FillRow(int row, const RigMotor& motor);
        std::optional<uint8_t> SelectedNode() const;
        void SelectionChanged();
        void OpenCommander();

        Rig& rig;
        QString commanderProgram;
        QTableWidget* table;
        QPushButton* add;
        QPushButton* open;
        QPushButton* powerOn;
        QPushButton* powerOff;
        QPushButton* powerCycle;
        QPushButton* pause;
        QPushButton* clearDisturbances;
        QPushButton* remove;
        std::map<uint8_t, QPointer<MotorWindow>> windows;
    };
}
