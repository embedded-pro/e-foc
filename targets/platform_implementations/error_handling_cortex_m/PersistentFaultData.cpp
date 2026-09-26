#include "targets/platform_implementations/error_handling_cortex_m/PersistentFaultData.hpp"
#include "infra/stream/StreamManipulators.hpp"

namespace application
{
    bool PersistentFaultData::IsValid() const
    {
        return magic == kMagicValid;
    }

    void PersistentFaultData::Invalidate()
    {
        magic = 0u;
    }

    void FormatFaultData(const PersistentFaultData& data, infra::BoundedString& out)
    {
        out.clear();
        infra::StringOutputStream stream(out, infra::softFail);

        stream << "\r\n\tStack frame:\r\n";
        stream << "\t\tR0  : 0x" << infra::hex << data.r0 << "\r\n";
        stream << "\t\tR1  : 0x" << infra::hex << data.r1 << "\r\n";
        stream << "\t\tR2  : 0x" << infra::hex << data.r2 << "\r\n";
        stream << "\t\tR3  : 0x" << infra::hex << data.r3 << "\r\n";
        stream << "\t\tR12 : 0x" << infra::hex << data.r12 << "\r\n";
        stream << "\t\tLR  : 0x" << infra::hex << data.lr << "\r\n";
        stream << "\t\tPC  : 0x" << infra::hex << data.pc << "\r\n";
        stream << "\t\tPSR : 0x" << infra::hex << data.psr << "\r\n";
        stream << "\tFSR/FAR:\r\n";
        stream << "\t\tCFSR : 0x" << infra::hex << data.cfsr << "\r\n";
        stream << "\t\tMMFAR: 0x" << infra::hex << data.mmfar << "\r\n";
        stream << "\t\tBFAR : 0x" << infra::hex << data.bfar << "\r\n";

        if (data.stackTraceCount > 0)
        {
            stream << "\tBacktrace:\r\n\t\t";
            const uint32_t count = (data.stackTraceCount < PersistentFaultData::kMaxStackTraceEntries)
                                       ? data.stackTraceCount
                                       : PersistentFaultData::kMaxStackTraceEntries;
            for (uint32_t i = 0; i < count; ++i)
                stream << " 0x" << infra::hex << infra::Width(8, '0') << data.stackTrace[i];
            stream << "\r\n";
        }

        // Acknowledge possible truncation (buffer may be smaller than worst-case output)
        (void)stream.Failed();
    }
}
