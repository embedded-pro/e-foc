#include "tools/virtual_motor/app/gui/AddMotorDialog.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace virtual_motor::app
{
    namespace
    {
        constexpr int descriptionFileItem{ 2 };
    }

    AddMotorDialog::AddMotorDialog(std::set<uint8_t> taken, QWidget* parent)
        : QDialog(parent)
        , taken(std::move(taken))
        , node(new QSpinBox(this))
        , plant(new QComboBox(this))
        , keepMemory(new QRadioButton("Keep its memory", this))
        , factoryFresh(new QRadioButton("Factory-fresh: erased memory, as a new board", this))
        , problem(new QLabel(this))
        , buttons(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this))
    {
        setWindowTitle("Add Virtual Motor");
        node->setRange(0, 255);
        node->setAccessibleName("Node");
        plant->addItem("Teknic M-2310P-LN-04K", "teknic");
        plant->addItem("Anaheim BLY172S-24V-4000", "anaheim");
        plant->addItem("Plant description file…");
        plant->setAccessibleName("Plant");
        keepMemory->setChecked(true);
        buttons->button(QDialogButtonBox::Ok)->setText("Add Motor");

        auto* form = new QFormLayout;
        form->addRow("&Node", node);
        form->addRow("&Plant", plant);
        form->addRow("Start", keepMemory);
        form->addRow("", factoryFresh);
        auto* layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addWidget(problem);
        layout->addWidget(buttons);

        connect(node, &QSpinBox::valueChanged, this, &AddMotorDialog::Validate);
        connect(plant, &QComboBox::activated, this, &AddMotorDialog::PlantChosen);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        Validate();
    }

    MotorSpec AddMotorDialog::Spec() const
    {
        const auto plantName = plant->currentIndex() == descriptionFileItem ? descriptionFile : plant->currentData().toString();
        return MotorSpec{ "default", static_cast<uint8_t>(node->value()), plantName.toStdString(), factoryFresh->isChecked() ? StartMode::factoryFresh : StartMode::keepMemory };
    }

    void AddMotorDialog::SetNode(uint8_t value)
    {
        node->setValue(value);
    }

    bool AddMotorDialog::CanAdd() const
    {
        return Problem().isEmpty();
    }

    QString AddMotorDialog::Problem() const
    {
        if (taken.contains(static_cast<uint8_t>(node->value())))
            return QString("Node %1 is already in the list; pick another node.").arg(node->value());
        if (plant->currentIndex() == descriptionFileItem && descriptionFile.isEmpty())
            return "Choose a plant description file.";
        return {};
    }

    void AddMotorDialog::PlantChosen(int index)
    {
        if (index == descriptionFileItem)
        {
            const auto file = QFileDialog::getOpenFileName(this, "Plant Description", {}, "Plant descriptions (*.bin);;All files (*)");
            if (!file.isEmpty())
            {
                descriptionFile = file;
                plant->setItemText(descriptionFileItem, QFileInfo(file).fileName());
            }
        }
        Validate();
    }

    void AddMotorDialog::Validate()
    {
        const auto text = Problem();
        problem->setText(text);
        problem->setVisible(!text.isEmpty());
        ui::backend::qt::StyleStatusLabel(*problem, ui::theme::StatusLevel::Warning);
        buttons->button(QDialogButtonBox::Ok)->setEnabled(text.isEmpty());
    }
}
