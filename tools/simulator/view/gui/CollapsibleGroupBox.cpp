#include "tools/simulator/view/gui/CollapsibleGroupBox.hpp"
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

    CollapsibleGroupBox::CollapsibleGroupBox(const QString& title, QWidget* parent)
        : QWidget(parent)
    {
        headerButton = QtOwned<QToolButton>(this);
        headerButton->setText(title);
        headerButton->setCheckable(true);
        headerButton->setChecked(true);
        headerButton->setStyleSheet("QToolButton { border: none; font-weight: bold; }");
        headerButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        headerButton->setArrowType(Qt::DownArrow);

        contentArea = QtOwned<QFrame>(this);
        contentArea->setFrameShape(QFrame::StyledPanel);

        auto* mainLayout = QtOwned<QVBoxLayout>(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(2);
        mainLayout->addWidget(headerButton);
        mainLayout->addWidget(contentArea);

        connect(headerButton, &QToolButton::toggled, this, [this](bool checked)
            {
                SetExpanded(checked);
            });
    }

    void CollapsibleGroupBox::SetContentLayout(QLayout* layout)
    {
        if (contentArea->layout() != nullptr)
            delete contentArea->layout();
        contentArea->setLayout(layout);
    }

    void CollapsibleGroupBox::SetExpanded(bool expanded)
    {
        headerButton->setChecked(expanded);
        headerButton->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        contentArea->setVisible(expanded);
    }

    bool CollapsibleGroupBox::IsExpanded() const
    {
        return headerButton->isChecked();
    }
}
