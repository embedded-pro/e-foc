#pragma once

#include "tools/can_commander/support/UpdateSession.hpp"
#include <QFrame>
#include <QLabel>
#include <QProgressBar>
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

        void Render(const UpdateState& state);

    signals:
        void InstallRequested();
        void CancelRequested();
        void SkipRequested();
        void DismissRequested();

    private:
        void RenderOffer(const UpdateState& state, const QString& version);
        void RenderDownload(const UpdateState& state, const QString& version);
        void RenderReady(const UpdateState& state, const QString& version);
        void RenderFailure(const UpdateState& state, const QString& version);
        void EnablePrimary(bool enabled);
        void ShowButtons(bool primary, bool notes, bool skip, bool cancel, bool dismiss);
        void OnPrimary();

        QLabel* message;
        QProgressBar* progress;
        QPushButton* primaryButton;
        QPushButton* notesButton;
        QPushButton* skipButton;
        QPushButton* cancelButton;
        QPushButton* dismissButton;
        QUrl notesUrl;
        QUrl downloadUrl;
        bool primaryInstalls{ false };
    };
}
