#include "core/supervision/ControlLoopStallMonitor.hpp"

namespace supervision
{
    ControlLoopStallMonitor::ControlLoopStallMonitor(application::DecimatingThreePhaseInverter& stage, const application::PlatformDiagnostics& diagnostics,
        const infra::Function<void(state_machine::FaultCode)>& onFault)
        : stage{ stage }
        , diagnostics{ diagnostics }
        , onFault{ onFault }
        , checkTimer{ checkPeriod, [this]()
            {
                Check(infra::Now());
            } }
    {}

    void ControlLoopStallMonitor::Check(infra::TimePoint now)
    {
        if (!stage.Started())
        {
            armedStart.reset();
            return;
        }

        const auto completed = diagnostics.ControlLoopCompletions();

        if (armedStart != stage.Starts() || completed != lastCompleted)
        {
            armedStart = stage.Starts();
            lastCompleted = completed;
            lastMoved = now;
            return;
        }

        // Judged on the time since movement was last seen, not on the previous check: a check the event
        // loop ran late is followed by one that can come within a control period of it.
        if (now - lastMoved < stallTime)
            return;

        armedStart.reset();
        stage.Stop();
        onFault(state_machine::FaultCode::controlLoopStall);
    }
}
