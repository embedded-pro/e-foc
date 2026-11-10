#pragma once

#include "tools/virtual_motor/app/logic/TraceSources.hpp"
#include "ui/core/PaintedView.hpp"
#include <functional>
#include <string>
#include <string_view>

namespace virtual_motor::app
{
    struct TimeRange
    {
        double start{ 0.0 };
        double end{ 0.0 };
    };

    class SourceView
        : public ui::PaintedView
    {
    public:
        virtual std::string_view Name() const = 0;
        virtual void Show(const TraceSource* source, TimeRange range, double origin) = 0;
        virtual void SetCursorsEnabled(bool enabled);
        virtual std::string CursorSummary() const;
        virtual void OnCursorsChanged(const std::function<void()>& callback);
    };
}
