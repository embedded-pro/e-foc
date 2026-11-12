#include "tools/virtual_motor/app/gui/AddMotorDialog.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
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
        constexpr double largestCurrentOffset{ 10.0 };
        constexpr std::array<const char*, 3> phases{ "a", "b", "c" };
    }

    AddMotorDialog::AddMotorDialog(std::set<uint8_t> taken, QWidget* parent)
        : QDialog(parent)
        , taken(std::move(taken))
        , node(new QSpinBox(this))
        , plant(new QComboBox(this))
        , keepMemory(new QRadioButton("Keep its memory", this))
        , factoryFresh(new QRadioButton("Factory-fresh: erased memory, as a new board", this))
        , calibrated(new QRadioButton("Calibrated for its plant: runs in every mode once aligned", this))
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
        form->addRow("", calibrated);
        form->addRow("Current offset", BuildCurrentOffset());
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
        return MotorSpec{ "default", static_cast<uint8_t>(node->value()), plantName.toStdString(), Start(),
            { static_cast<float>(currentOffset[0]->value()), static_cast<float>(currentOffset[1]->value()), static_cast<float>(currentOffset[2]->value()) } };
    }

    StartMode AddMotorDialog::Start() const
    {
        if (factoryFresh->isChecked())
            return StartMode::factoryFresh;
        if (calibrated->isChecked())
            return StartMode::calibrated;
        return StartMode::keepMemory;
    }

    QWidget* AddMotorDialog::BuildCurrentOffset()
    {
        auto* row = new QWidget(this);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        for (std::size_t phase = 0; phase != currentOffset.size(); ++phase)
        {
            auto* spin = new QDoubleSpinBox(row);
            spin->setRange(-largestCurrentOffset, largestCurrentOffset);
            spin->setDecimals(4);
            spin->setSingleStep(0.001);
            spin->setPrefix(QString("%1 ").arg(phases[phase]));
            spin->setSuffix(" A");
            spin->setAccessibleName(QString("Current offset %1").arg(phases[phase]));
            spin->setToolTip("The offset the current-sense chain adds to the measured phase current, as a board's conditioning circuit does; fixed for the motor's life");
            currentOffset[phase] = spin;
            layout->addWidget(spin);
        }
        return row;
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
