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

        stream << "Stack frame:\n";
        stream << " R0  : 0x" << infra::hex << infra::Width(8, '0') << data.r0 << "\n";
        stream << " R1  : 0x" << infra::hex << infra::Width(8, '0') << data.r1 << "\n";
        stream << " R2  : 0x" << infra::hex << infra::Width(8, '0') << data.r2 << "\n";
        stream << " R3  : 0x" << infra::hex << infra::Width(8, '0') << data.r3 << "\n";
        stream << " R12 : 0x" << infra::hex << infra::Width(8, '0') << data.r12 << "\n";
        stream << " LR  : 0x" << infra::hex << infra::Width(8, '0') << data.lr << "\n";
        stream << " PC  : 0x" << infra::hex << infra::Width(8, '0') << data.pc << "\n";
        stream << " PSR : 0x" << infra::hex << infra::Width(8, '0') << data.psr << "\n";
        stream << "FSR/FAR:\n";
        stream << " CFSR : 0x" << infra::hex << infra::Width(8, '0') << data.cfsr << "\n";
        stream << " MMFAR: 0x" << infra::hex << infra::Width(8, '0') << data.mmfar << "\n";
        stream << " BFAR : 0x" << infra::hex << infra::Width(8, '0') << data.bfar << "\n";

        if (data.stackTraceCount > 0)
        {
            stream << "Backtrace:";
            const uint32_t count = (data.stackTraceCount < PersistentFaultData::kMaxStackTraceEntries)
                                       ? data.stackTraceCount
                                       : PersistentFaultData::kMaxStackTraceEntries;
            for (uint32_t i = 0; i < count; ++i)
                stream << " 0x" << infra::hex << infra::Width(8, '0') << data.stackTrace[i];
            stream << "\n";
        }

        // Acknowledge possible truncation (buffer may be smaller than worst-case output)
        (void)stream.Failed();
    }
}
