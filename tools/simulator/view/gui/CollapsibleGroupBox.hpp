#pragma once

#include <QFrame>
#include <QLayout>
#include <QString>
#include <QToolButton>
#include <QWidget>

namespace simulator
{
    // Lightweight collapsible group container with a clickable header. Mirrors the
    // affordance of QGroupBox but lets the user hide bulky content (e.g. plot stacks)
    // to reclaim vertical space.
    class CollapsibleGroupBox
        : public QWidget
    {
        Q_OBJECT

    public:
        explicit CollapsibleGroupBox(const QString& title, QWidget* parent = nullptr);

        void SetContentLayout(QLayout* layout);
        void SetExpanded(bool expanded);
        bool IsExpanded() const;

    private:
        QToolButton* headerButton;
        QFrame* contentArea;
    };
}
