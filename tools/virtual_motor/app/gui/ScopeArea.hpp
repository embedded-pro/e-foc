#pragma once

#include "tools/virtual_motor/app/logic/EncoderView.hpp"
#include "tools/virtual_motor/app/logic/Rig.hpp"
#include "tools/virtual_motor/app/logic/TraceView.hpp"
#include <QWidget>
#include <memory>
#include <optional>
#include <vector>

class QComboBox;
class QLabel;
class QPushButton;
class QTabWidget;
class QTimer;

namespace virtual_motor::app
{
    enum class ScopeMode : uint8_t
    {
        live,
        frozen,
        capture,
    };

    class ScopeArea
        : public QWidget
    {
        Q_OBJECT

    public:
        ScopeArea(RigMotor& motor, EncoderGeometry geometry, QWidget* parent = nullptr);

        void AddPage(QWidget* page, const QString& title);
        bool ShowsTrace() const;
        ScopeMode Mode() const;
        SourceView* CurrentView() const;
        void ShowCapture(std::optional<uint8_t> signal);
        void Refresh();

    signals:
        void TraceShownChanged();

    private:
        QWidget* BuildToolbar();
        void ConnectToolbar();
        void SetMode(ScopeMode newMode);
        void ToggleFreeze();
        void SaveCapture();
        void AddView(std::unique_ptr<SourceView> view);
        void Tick();
        void ShowOn(SourceView& view);
        void RefreshToolbar();
        uint32_t ControlRate() const;
        double SpanSeconds() const;

        RigMotor& motor;
        QTabWidget* tabs;
        QComboBox* span;
        QPushButton* freeze;
        QPushButton* live;
        QPushButton* save;
        QLabel* modeLabel;
        QLabel* cursors;
        QTimer* timer;
        std::vector<std::unique_ptr<SourceView>> views;
        ScopeMode mode{ ScopeMode::live };
        std::optional<TraceHistory> frozen;
        std::optional<HistoryTrace> liveTrace;
        std::optional<HistoryTrace> frozenTrace;
        std::optional<CaptureTrace> captureTrace;
        uint64_t shownRevision{ 0 };
        const SourceView* shownView{ nullptr };
        bool stale{ true };
    };
}
