#pragma once

#include "tools/virtual_motor/app/logic/ProfileFile.hpp"
#include "tools/virtual_motor/app/logic/Rig.hpp"
#include <QGroupBox>
#include <optional>

class QLabel;
class QPlainTextEdit;
class QPushButton;

namespace virtual_motor::app
{
    class ProfilePanel
        : public QGroupBox
    {
        Q_OBJECT

    public:
        ProfilePanel(RigMotor& motor, QWidget* parent = nullptr);

        void Refresh();
        void ShowRefusal(const port::Refused& refused);
        void SetText(const QString& text);

    private:
        QWidget* BuildButtons();
        void Start();
        void Load();
        void Save();
        uint32_t ControlRate() const;

        RigMotor& motor;
        QPlainTextEdit* editor;
        QPushButton* load;
        QPushButton* save;
        QPushButton* start;
        QPushButton* stop;
        QLabel* status;
        std::optional<Profile> started;
        QString problem;
    };
}
