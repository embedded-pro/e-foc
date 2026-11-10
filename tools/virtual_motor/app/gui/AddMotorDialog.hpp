#pragma once

#include "tools/virtual_motor/app/logic/MotorSpec.hpp"
#include <QDialog>
#include <set>

class QComboBox;
class QDialogButtonBox;
class QLabel;
class QRadioButton;
class QSpinBox;

namespace virtual_motor::app
{
    class AddMotorDialog
        : public QDialog
    {
        Q_OBJECT

    public:
        AddMotorDialog(std::set<uint8_t> taken, QWidget* parent = nullptr);

        MotorSpec Spec() const;
        void SetNode(uint8_t node);
        bool CanAdd() const;
        QString Problem() const;

    private:
        void PlantChosen(int index);
        void Validate();

        std::set<uint8_t> taken;
        QSpinBox* node;
        QComboBox* plant;
        QRadioButton* keepMemory;
        QRadioButton* factoryFresh;
        QLabel* problem;
        QDialogButtonBox* buttons;
        QString descriptionFile;
    };
}
