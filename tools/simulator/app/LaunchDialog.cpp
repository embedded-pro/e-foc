#include "tools/simulator/app/LaunchDialog.hpp"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>
#include <memory>

namespace simulator
{
    namespace
    {
        template<typename T, typename... Args>
        T* QtOwned(Args&&... args)
        {
            return std::make_unique<T>(std::forward<Args>(args)...).release();
        }
    }

    LaunchDialog::LaunchDialog(QWidget* parent)
        : QDialog(parent)
    {
        setWindowTitle("e-foc Simulator — Select Controller");
        setModal(true);

        auto* layout = QtOwned<QVBoxLayout>(this);
        layout->addWidget(QtOwned<QLabel>("Choose the controller to simulate:", this));

        combo = QtOwned<QComboBox>(this);
        combo->addItem("Torque (current) control", static_cast<int>(ControllerKind::Torque));
        combo->addItem("Speed control", static_cast<int>(ControllerKind::Speed));
        combo->addItem("Position control", static_cast<int>(ControllerKind::Position));
        layout->addWidget(combo);

        auto* buttons = QtOwned<QDialogButtonBox>(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        layout->addWidget(buttons);

        connect(buttons, &QDialogButtonBox::accepted, this, [this]()
            {
                selected = static_cast<ControllerKind>(combo->currentData().toInt());
                accept();
            });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    std::optional<ControllerKind> LaunchDialog::Selected() const
    {
        return selected;
    }
}
