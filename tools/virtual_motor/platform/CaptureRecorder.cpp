#include "tools/virtual_motor/platform/CaptureRecorder.hpp"
#include <algorithm>

namespace virtual_motor
{
    void CaptureRecorder::Report(InstrumentReports& reportsTo)
    {
        reports = &reportsTo;
    }

    std::optional<port::RefusalReason> CaptureRecorder::Arm(const port::CaptureRequest& request, uint64_t period)
    {
        if (request.length == 0 || request.length > port::maxCapturePeriods || request.preTriggerPercent > 100)
            return port::RefusalReason::outOfRange;
        if (auto refusal = TriggerWatch::Check(request.trigger))
            return refusal;

        Cancel(period);
        preTarget = std::min<std::size_t>(request.length * request.preTriggerPercent / 100u, request.length - 1u);
        postTarget = request.length - preTarget;
        before.clear();
        recorded.clear();
        watch.Arm(request.trigger);
        state = State::armed;
        ++number;
        Event(port::CaptureEventKind::armed, period);
        return std::nullopt;
    }

    void CaptureRecorder::Cancel(uint64_t period)
    {
        if (state == State::idle)
            return;

        state = State::idle;
        Event(port::CaptureEventKind::cancelled, period);
    }

    bool CaptureRecorder::Active() const
    {
        return state != State::idle;
    }

    void CaptureRecorder::Observe(uint64_t period, const port::Sample& sample, uint8_t leds, bool protection)
    {
        if (state == State::armed)
        {
            if (watch.Fires(sample, leds, protection))
                Triggered(period, sample);
            else
            {
                before.push_back(sample);
                if (before.size() > preTarget)
                    before.pop_front();
            }
        }
        else if (state == State::recording)
            recorded.push_back(sample);

        if (state == State::recording && recorded.size() == preTrigger + postTarget)
            Deliver();
    }

    void CaptureRecorder::Event(port::CaptureEventKind event, uint64_t period)
    {
        if (reports != nullptr)
            reports->ReportCaptureEvent(port::CaptureEvent{ number, event, period });
    }

    void CaptureRecorder::Triggered(uint64_t period, const port::Sample& sample)
    {
        preTrigger = before.size();
        firstPeriod = period - preTrigger;
        recorded.assign(before.begin(), before.end());
        recorded.push_back(sample);
        before.clear();
        state = State::recording;
        Event(port::CaptureEventKind::triggered, period);
    }

    void CaptureRecorder::Deliver()
    {
        state = State::idle;
        if (reports == nullptr)
            return;

        const auto length = static_cast<uint16_t>(recorded.size());
        for (std::size_t index = 0; index < recorded.size(); index += port::captureChunkPeriods)
        {
            const auto end = std::min(index + port::captureChunkPeriods, recorded.size());
            port::CaptureChunk chunk{ number, length, static_cast<uint16_t>(preTrigger), firstPeriod, static_cast<uint16_t>(index), {} };
            chunk.periods.assign(recorded.begin() + static_cast<std::ptrdiff_t>(index), recorded.begin() + static_cast<std::ptrdiff_t>(end));
            reports->ReportCaptureChunk(chunk);
        }
        recorded.clear();
    }
}
