#include "tools/can_commander/gui/UpdateBanner.hpp"
#include "tools/can_commander/support/ApplicationInfo.hpp"
#include "ui/backend/qt/QtConversions.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include "ui/theme/Theme.hpp"
#include <QDesktopServices>
#include <QHBoxLayout>

namespace tool
{
    UpdateBanner::UpdateBanner(QWidget* parent)
        : QFrame(parent)
    {
        setFrameShape(QFrame::StyledPanel);
        setAccessibleName("Update available");

        auto* layout = new QHBoxLayout(this);
        message = new QLabel();
        message->setWordWrap(true);
        ui::backend::qt::StyleStatusLabel(*message, ui::theme::StatusLevel::Ok);
        layout->addWidget(message, 1);

        downloadButton = new QPushButton("Do&wnload Installer");
        ui::backend::qt::StyleButton(*downloadButton, ui::theme::ButtonRole::Primary);
        notesButton = new QPushButton("&Release Notes");
        auto* dismissButton = new QPushButton("Later");
        dismissButton->setToolTip("Hide this message until the next start");
        layout->addWidget(downloadButton);
        layout->addWidget(notesButton);
        layout->addWidget(dismissButton);

        connect(downloadButton, &QPushButton::clicked, this, [this]()
            {
                QDesktopServices::openUrl(downloadUrl);
            });
        connect(notesButton, &QPushButton::clicked, this, [this]()
            {
                QDesktopServices::openUrl(notesUrl);
            });
        connect(dismissButton, &QPushButton::clicked, this, &QWidget::hide);

        hide();
    }

    void UpdateBanner::ShowRelease(const ReleaseInfo& release)
    {
        notesUrl = release.pageUrl;
        downloadUrl = release.installerUrl.value_or(release.pageUrl);

        using ui::backend::qt::ToQt;
        const auto latest = QString::fromStdString(release.version.ToString());
        message->setText(QString("%1 %2 is available — you have %3.").arg(ToQt(application::displayName), latest, ToQt(application::version)));
        downloadButton->setText(release.installerUrl ? "Do&wnload Installer" : "Open Do&wnload Page");
        show();
    }
}
