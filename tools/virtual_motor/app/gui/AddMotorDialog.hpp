#pragma once

#include "tools/virtual_motor/app/logic/MotorSpec.hpp"
#include <QDialog>
#include <array>
#include <set>

class QComboBox;
class QDoubleSpinBox;
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
        QWidget* BuildCurrentOffset();
        StartMode Start() const;
        void PlantChosen(int index);
        void Validate();

        std::set<uint8_t> taken;
        QSpinBox* node;
        QComboBox* plant;
        QRadioButton* keepMemory;
        QRadioButton* factoryFresh;
        QRadioButton* calibrated;
        std::array<QDoubleSpinBox*, 3> currentOffset{};
        QLabel* problem;
        QDialogButtonBox* buttons;
        QString descriptionFile;
    };
}
