#pragma once

#include "ui/theme/Theme.hpp"
#include <QFormLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QString>
#include <QWidget>

namespace tool
{
    struct FirmwareActions
    {
        bool openPack{ false };
        bool download{ false };
        bool apply{ false };
        bool discard{ false };
        bool cancel{ false };
        bool tryAgain{ false };
        QString downloadReason;
        QString applyReason;
    };

    class FirmwareTab
        : public QWidget
    {
        Q_OBJECT

    public:
        explicit FirmwareTab(QWidget* parent = nullptr);

        void ShowFirmware(const QString& running, const QString& staged, const QString& release, const QString& pack);
        void ShowStatus(const QString& text, ui::theme::StatusLevel level);
        void ShowProgress(qint64 done, qint64 total, const QString& format);
        void ShowBusy();
        void HideProgress();
        void SetActions(const FirmwareActions& actions);

    signals:
        void OpenPackRequested();
        void DownloadRequested();
        void ApplyRequested();
        void DiscardRequested();
        void CancelRequested();
        void TryAgainRequested();

    private:
        QWidget* CreateIdentity();
        QWidget* CreateActions();
        static void Enable(QPushButton& button, bool enabled, const QString& hint, const QString& reason);

        QLabel* status{ nullptr };
        QLabel* running{ nullptr };
        QLabel* staged{ nullptr };
        QLabel* release{ nullptr };
        QLabel* pack{ nullptr };
        QProgressBar* progress{ nullptr };
        QPushButton* openPackButton{ nullptr };
        QPushButton* downloadButton{ nullptr };
        QPushButton* applyButton{ nullptr };
        QPushButton* discardButton{ nullptr };
        QPushButton* cancelButton{ nullptr };
        QPushButton* tryAgainButton{ nullptr };
    };
}
