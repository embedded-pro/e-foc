#include "tools/can_commander/gui/FirmwareTab.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace tool
{
    namespace
    {
        constexpr const char* placeholder = "—";
        constexpr const char* openPackHint = "Choose an upgrade pack file, for a development build";
        constexpr const char* downloadHint = "Send the pack to the controller, which checks it and keeps it staged; nothing is installed yet";
        constexpr const char* applyHint = "Restart the controller so that its bootloader installs the staged pack";
        constexpr const char* discardHint = "Tell the controller to forget the staged pack";
        constexpr const char* cancelHint = "Stop the download; nothing is staged";
        constexpr const char* tryAgainHint = "Download the pack again from the beginning, after confirming";

        QLabel* ValueLabel(const QString& accessibleName)
        {
            auto* label = new QLabel(placeholder);
            label->setWordWrap(true);
            label->setTextInteractionFlags(Qt::TextSelectableByMouse);
            label->setAccessibleName(accessibleName);
            return label;
        }
    }

    FirmwareTab::FirmwareTab(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        status = new QLabel();
        status->setWordWrap(true);
        ui::backend::qt::StyleStatusLabel(*status, ui::theme::StatusLevel::Neutral);
        layout->addWidget(status);
        layout->addWidget(CreateIdentity());

        progress = new QProgressBar();
        progress->setAccessibleName("Firmware transfer progress");
        progress->setVisible(false);
        layout->addWidget(progress);

        layout->addWidget(CreateActions());
        layout->addStretch();
        SetActions({});
    }

    QWidget* FirmwareTab::CreateIdentity()
    {
        auto* group = new QGroupBox("Firmware");
        auto* form = new QFormLayout(group);
        running = ValueLabel("Running firmware");
        staged = ValueLabel("Staged pack");
        release = ValueLabel("Latest release");
        pack = ValueLabel("Pack to download");
        form->addRow("Running:", running);
        form->addRow("Staged:", staged);
        form->addRow("Latest release:", release);
        form->addRow("Pack:", pack);
        return group;
    }

    QWidget* FirmwareTab::CreateActions()
    {
        auto* actions = new QWidget();
        auto* layout = new QHBoxLayout(actions);
        layout->setContentsMargins(0, 0, 0, 0);

        openPackButton = new QPushButton("Open Pac&k File…");
        downloadButton = new QPushButton("&Download to Controller…");
        cancelButton = new QPushButton("Ca&ncel");
        tryAgainButton = new QPushButton("&Try Again");
        applyButton = new QPushButton("&Apply Firmware…");
        discardButton = new QPushButton("D&iscard");

        for (auto* button : { openPackButton, downloadButton, cancelButton, tryAgainButton, applyButton, discardButton })
            layout->addWidget(button);
        layout->addStretch();

        connect(openPackButton, &QPushButton::clicked, this, &FirmwareTab::OpenPackRequested);
        connect(downloadButton, &QPushButton::clicked, this, &FirmwareTab::DownloadRequested);
        connect(cancelButton, &QPushButton::clicked, this, &FirmwareTab::CancelRequested);
        connect(tryAgainButton, &QPushButton::clicked, this, &FirmwareTab::TryAgainRequested);
        connect(applyButton, &QPushButton::clicked, this, &FirmwareTab::ApplyRequested);
        connect(discardButton, &QPushButton::clicked, this, &FirmwareTab::DiscardRequested);
        return actions;
    }

    void FirmwareTab::ShowFirmware(const QString& runningText, const QString& stagedText, const QString& releaseText, const QString& packText)
    {
        running->setText(runningText.isEmpty() ? QString(placeholder) : runningText);
        staged->setText(stagedText.isEmpty() ? QString(placeholder) : stagedText);
        release->setText(releaseText.isEmpty() ? QString(placeholder) : releaseText);
        pack->setText(packText.isEmpty() ? QString(placeholder) : packText);
    }

    void FirmwareTab::ShowStatus(const QString& text, ui::theme::StatusLevel level)
    {
        status->setText(text);
        ui::backend::qt::StyleStatusLabel(*status, level);
    }

    void FirmwareTab::ShowProgress(qint64 done, qint64 total, const QString& format)
    {
        progress->setRange(0, 1000);
        progress->setValue(total > 0 ? static_cast<int>(done * 1000 / total) : 0);
        progress->setFormat(format);
        progress->setVisible(true);
    }

    void FirmwareTab::ShowBusy()
    {
        progress->setRange(0, 0);
        progress->setVisible(true);
    }

    void FirmwareTab::HideProgress()
    {
        progress->setVisible(false);
    }

    void FirmwareTab::SetActions(const FirmwareActions& actions)
    {
        Enable(*openPackButton, actions.openPack, openPackHint, actions.downloadReason);
        Enable(*downloadButton, actions.download, downloadHint, actions.downloadReason);
        Enable(*applyButton, actions.apply, applyHint, actions.applyReason);
        Enable(*discardButton, actions.discard, discardHint, actions.applyReason);
        cancelButton->setVisible(actions.cancel);
        cancelButton->setToolTip(cancelHint);
        tryAgainButton->setVisible(actions.tryAgain);
        tryAgainButton->setToolTip(tryAgainHint);
        tryAgainButton->setEnabled(actions.download);
    }

    void FirmwareTab::Enable(QPushButton& button, bool enabled, const QString& hint, const QString& reason)
    {
        button.setEnabled(enabled);
        button.setToolTip(enabled || reason.isEmpty() ? hint : reason);
    }
}
