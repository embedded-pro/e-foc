#pragma once

#include "tools/can_commander/support/ReleaseFeed.hpp"
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QUrl>

namespace tool
{
    class UpdateBanner
        : public QFrame
    {
        Q_OBJECT

    public:
        explicit UpdateBanner(QWidget* parent = nullptr);

        void ShowRelease(const ReleaseInfo& release);

    private:
        QLabel* message;
        QPushButton* downloadButton;
        QPushButton* notesButton;
        QUrl downloadUrl;
        QUrl notesUrl;
    };
}
