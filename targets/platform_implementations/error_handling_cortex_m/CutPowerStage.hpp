#pragma once

namespace application
{
    // Brings the power stage down from a context in which application state cannot be trusted: a HardFault,
    // whose cause may be that state, or a watchdog expiry, which means the code owning it stopped running.
    // An implementation writes the peripheral registers through addresses fixed at compile time and reads no
    // writable memory — no object, no global pointer, no virtual call.
    //
    // Every platform must define it. There is deliberately no weak fallback: a weak definition would be the
    // one the linker settles on, leaving a hook that is called on every fault and cuts nothing, whereas a
    // platform that forgets this definition fails to link. A platform that drives no bridge defines it empty.
    void CutPowerStage();
}
