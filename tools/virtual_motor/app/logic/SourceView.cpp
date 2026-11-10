#include "tools/virtual_motor/app/logic/SourceView.hpp"

namespace virtual_motor::app
{
    void SourceView::SetCursorsEnabled(bool)
    {}

    std::string SourceView::CursorSummary() const
    {
        return {};
    }

    void SourceView::OnCursorsChanged(const std::function<void()>&)
    {}
}
