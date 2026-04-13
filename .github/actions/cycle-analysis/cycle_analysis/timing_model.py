"""
Cortex-M instruction timing model.

Contains the timing table, conditional suffix list, exception overhead
constants, and the three instruction-classification helpers used by the
parser and analysis modules.

Reference: ARM Cortex-M4 Technical Reference Manual (DDI0439D) §3.
"""

from __future__ import annotations

# ──────────────────────────────────────────────────────────────────────────────
# Cortex-M4 Instruction Timing Table  (min, max) cycles
# ──────────────────────────────────────────────────────────────────────────────

TIMING: dict[str, tuple[int, int]] = {
    # ── Integer Data Processing ───────────────────────────────────────────
    "add": (1, 1), "adds": (1, 1), "adc": (1, 1), "adcs": (1, 1),
    "sub": (1, 1), "subs": (1, 1), "sbc": (1, 1), "sbcs": (1, 1),
    "rsb": (1, 1), "rsbs": (1, 1), "rsc": (1, 1),
    "and": (1, 1), "ands": (1, 1), "orr": (1, 1), "orrs": (1, 1),
    "eor": (1, 1), "eors": (1, 1), "bic": (1, 1), "bics": (1, 1),
    "orn": (1, 1), "orns": (1, 1),
    "mov": (1, 1), "movs": (1, 1), "mvn": (1, 1), "mvns": (1, 1),
    "movw": (1, 1), "movt": (1, 1),
    "cmp": (1, 1), "cmn": (1, 1), "tst": (1, 1), "teq": (1, 1),
    # ── Shift / Rotate ────────────────────────────────────────────────────
    "lsl": (1, 1), "lsls": (1, 1), "lsr": (1, 1), "lsrs": (1, 1),
    "asr": (1, 1), "asrs": (1, 1), "ror": (1, 1), "rors": (1, 1),
    "rrx": (1, 1),
    # ── Multiply ──────────────────────────────────────────────────────────
    "mul": (1, 1), "muls": (1, 1), "mla": (1, 1), "mls": (1, 1),
    "smull": (1, 1), "umull": (1, 1), "smlal": (1, 1), "umlal": (1, 1),
    "smulbb": (1, 1), "smulbt": (1, 1), "smultb": (1, 1), "smultt": (1, 1),
    "smlabb": (1, 1), "smlabt": (1, 1), "smlatb": (1, 1), "smlatt": (1, 1),
    "smulwb": (1, 1), "smulwt": (1, 1), "smlawb": (1, 1), "smlawt": (1, 1),
    # ── Division ──────────────────────────────────────────────────────────
    "sdiv": (2, 12), "udiv": (2, 12),
    # ── Saturating / Packing / Extend ─────────────────────────────────────
    "ssat": (1, 1), "usat": (1, 1),
    "qadd": (1, 1), "qsub": (1, 1), "qdadd": (1, 1), "qdsub": (1, 1),
    "pkhbt": (1, 1), "pkhtb": (1, 1),
    "sxtb": (1, 1), "sxth": (1, 1), "uxtb": (1, 1), "uxth": (1, 1),
    "sxtab": (1, 1), "sxtah": (1, 1), "uxtab": (1, 1), "uxtah": (1, 1),
    "sxtb16": (1, 1), "uxtb16": (1, 1),
    "sbfx": (1, 1), "ubfx": (1, 1), "bfi": (1, 1), "bfc": (1, 1),
    # ── Bit manipulation ─────────────────────────────────────────────────
    "rev": (1, 1), "rev16": (1, 1), "revsh": (1, 1), "rbit": (1, 1),
    "clz": (1, 1),
    # ── Load single ──────────────────────────────────────────────────────
    "ldr": (2, 2), "ldrb": (2, 2), "ldrh": (2, 2),
    "ldrsb": (2, 2), "ldrsh": (2, 2), "ldrd": (2, 2),
    "ldr.w": (2, 2), "ldrb.w": (2, 2), "ldrh.w": (2, 2),
    # ── Store single ─────────────────────────────────────────────────────
    "str": (2, 2), "strb": (2, 2), "strh": (2, 2), "strd": (2, 2),
    "str.w": (2, 2), "strb.w": (2, 2), "strh.w": (2, 2),
    # ── Load / Store multiple ────────────────────────────────────────────
    "ldm": (1, 2), "ldmia": (1, 2), "ldmdb": (1, 2),
    "stm": (1, 2), "stmia": (1, 2), "stmdb": (1, 2),
    "push": (1, 2), "pop": (1, 2),
    # ── Branch ───────────────────────────────────────────────────────────
    "b": (1, 4), "b.w": (1, 4), "b.n": (1, 4),
    "bx": (1, 4),
    "bl": (1, 4), "blx": (1, 4),
    "bne": (1, 4), "bne.n": (1, 4), "bne.w": (1, 4),
    "beq": (1, 4), "beq.n": (1, 4), "beq.w": (1, 4),
    "blt": (1, 4), "blt.n": (1, 4), "blt.w": (1, 4),
    "bgt": (1, 4), "bgt.n": (1, 4), "bgt.w": (1, 4),
    "ble": (1, 4), "ble.n": (1, 4), "ble.w": (1, 4),
    "bge": (1, 4), "bge.n": (1, 4), "bge.w": (1, 4),
    "bhi": (1, 4), "bhi.n": (1, 4), "bhi.w": (1, 4),
    "bls": (1, 4), "bls.n": (1, 4), "bls.w": (1, 4),
    "bcc": (1, 4), "bcc.n": (1, 4), "bcc.w": (1, 4),
    "bcs": (1, 4), "bcs.n": (1, 4), "bcs.w": (1, 4),
    "bmi": (1, 4), "bmi.n": (1, 4), "bmi.w": (1, 4),
    "bpl": (1, 4), "bpl.n": (1, 4), "bpl.w": (1, 4),
    "bvs": (1, 4), "bvs.n": (1, 4), "bvs.w": (1, 4),
    "bvc": (1, 4), "bvc.n": (1, 4), "bvc.w": (1, 4),
    "cbz": (1, 4), "cbnz": (1, 4),
    "tbb": (2, 4), "tbh": (2, 4),
    # ── IT (conditional execution) ───────────────────────────────────────
    "it": (1, 1), "ite": (1, 1), "itt": (1, 1),
    "ittt": (1, 1), "itte": (1, 1), "itet": (1, 1), "itee": (1, 1),
    # ── Misc ─────────────────────────────────────────────────────────────
    "nop": (1, 1), "svc": (1, 1), "bkpt": (1, 1),
    "dmb": (1, 4), "dsb": (1, 4), "isb": (1, 4),
    "cpsie": (1, 2), "cpsid": (1, 2),
    "mrs": (2, 2), "msr": (2, 2),
    # ── FPv4-SP-D16  (single-precision HW FPU) ──────────────────────────
    "vadd.f32": (1, 1), "vsub.f32": (1, 1),
    "vmul.f32": (1, 1), "vnmul.f32": (1, 1),
    "vfma.f32": (1, 1), "vfms.f32": (1, 1),
    "vfnma.f32": (1, 1), "vfnms.f32": (1, 1),
    "vmla.f32": (3, 3), "vmls.f32": (3, 3),
    "vneg.f32": (1, 1), "vabs.f32": (1, 1),
    "vcmp.f32": (1, 1), "vcmpe.f32": (1, 1),
    "vmov": (1, 1), "vmov.f32": (1, 1), "vmov.f64": (1, 1),
    "vcvt.f32.s32": (1, 1), "vcvt.s32.f32": (1, 1),
    "vcvt.f32.u32": (1, 1), "vcvt.u32.f32": (1, 1),
    "vcvt.f64.f32": (1, 1), "vcvt.f32.f64": (1, 1),
    "vcvtr.s32.f32": (1, 1), "vcvtr.u32.f32": (1, 1),
    "vcvt.f32.s16": (1, 1), "vcvt.f32.u16": (1, 1),
    "vsqrt.f32": (14, 14),
    "vdiv.f32": (14, 14),
    "vldr": (2, 2), "vldr.32": (2, 2),
    "vstr": (2, 2), "vstr.32": (2, 2),
    "vpush": (1, 2), "vpop": (1, 2),
    "vmrs": (1, 1), "vmsr": (1, 1),
    "vldm": (1, 2), "vldmia": (1, 2), "vldmdb": (1, 2),
    "vstm": (1, 2), "vstmia": (1, 2), "vstmdb": (1, 2),
    "vsel.f32": (1, 1),
    "vmaxnm.f32": (1, 1), "vminnm.f32": (1, 1),
}

COND_SUFFIXES: list[str] = [
    "eq", "ne", "cs", "hs", "cc", "lo",
    "mi", "pl", "vs", "vc", "hi", "ls",
    "ge", "lt", "gt", "le", "al",
]

# ──────────────────────────────────────────────────────────────────────────────
# Cortex-M exception entry/exit overhead
# ──────────────────────────────────────────────────────────────────────────────

EXCEPTION_OVERHEAD: dict[str, dict[str, int]] = {
    "m4": {
        "entry_min": 12,     # Integer-only context (R0-R3,R12,LR,PC,xPSR)
        "entry_max": 29,     # With FPU lazy stacking (+ S0-S15, FPSCR)
        "exit_min": 12,
        "exit_max": 29,
        "tail_chain": 6,     # Tail-chained exception
    },
    "m33": {
        "entry_min": 12,
        "entry_max": 29,
        "exit_min": 12,
        "exit_max": 29,
        "tail_chain": 6,
    },
    "m0": {
        "entry_min": 16,
        "entry_max": 16,
        "exit_min": 16,
        "exit_max": 16,
        "tail_chain": 6,
    },
}


# ──────────────────────────────────────────────────────────────────────────────
# Instruction helpers
# ──────────────────────────────────────────────────────────────────────────────

def normalize_mnemonic(mnemonic: str) -> str:
    m = mnemonic.lower().strip()
    if m in TIMING:
        return m
    for suffix in [".w", ".n"]:
        if (m + suffix) in TIMING:
            return m + suffix
    base = m.rstrip(".wn")
    if base in TIMING:
        return base
    if len(m) > 3:
        for cond in COND_SUFFIXES:
            if m.endswith(cond):
                base_cond = m[: -len(cond)]
                if base_cond in TIMING:
                    return base_cond
    return m


def classify_instruction(mnemonic: str) -> str:
    m = mnemonic.lower()
    if m.startswith("v"):
        if any(x in m for x in ["ldr", "str", "push", "pop", "ldm", "stm"]):
            return "load_store"
        return "fpu"
    if any(m.startswith(x) for x in ["ldr", "str", "ldm", "stm", "push", "pop"]):
        return "load_store"
    if any(m.startswith(x) for x in ["b", "bl", "bx", "cb", "tb"]):
        if m in ("bic", "bics", "bfi", "bfc"):
            return "alu"
        return "branch"
    if any(m.startswith(x) for x in [
        "mul", "mla", "mls", "smul", "umul", "smlal", "umlal", "sdiv", "udiv"
    ]):
        return "mul_div"
    return "alu"


def get_timing(mnemonic: str) -> tuple[int, int]:
    normalized = normalize_mnemonic(mnemonic)
    if normalized in TIMING:
        return TIMING[normalized]
    m = mnemonic.lower()
    if m.startswith("v"):
        return (1, 2)
    if "ldr" in m or "str" in m:
        return (2, 2)
    if m.startswith("b") and m not in ("bic", "bics", "bfi", "bfc"):
        return (1, 4)
    return (1, 1)
