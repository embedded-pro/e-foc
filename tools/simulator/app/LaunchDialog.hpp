#pragma once

#include "tools/simulator/app/ControllerKind.hpp"
#include <QDialog>
#include <optional>

class QComboBox;

namespace simulator
{
    class LaunchDialog
        : public QDialog
    {
        Q_OBJECT

    public:
        explicit LaunchDialog(QWidget* parent = nullptr);

        std::optional<ControllerKind> Selected() const;

    private:
        QComboBox* combo;
        std::optional<ControllerKind> selected;
    };
}
