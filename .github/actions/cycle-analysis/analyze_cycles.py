#!/usr/bin/env python3
"""
ARM Cortex-M Cycle Estimation — Configurable Critical-Path Analysis.

Parses arm-none-eabi-objdump disassembly and estimates machine cycles for a
user-defined critical path through an ARM Cortex-M binary.  The path stages
are fully driven by a JSON configuration file — no application-specific
knowledge is hard-coded in this script.

Timing model: Cortex-M4 (ARMv7E-M, FPv4-SP-D16) based on the ARM DDI0439D TRM.
Configurable via JSON for any application / MCU combination.

Usage:
    python3 analyze_cycles.py config.json --elf path/to/app.elf --output-dir out/
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field
from typing import Optional

# ──────────────────────────────────────────────────────────────────────────────
# Cortex-M4 Instruction Timing Table  (min, max) cycles
# Reference: ARM Cortex-M4 Technical Reference Manual (DDI0439D) §3
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

COND_SUFFIXES = [
    "eq", "ne", "cs", "hs", "cc", "lo",
    "mi", "pl", "vs", "vc", "hi", "ls",
    "ge", "lt", "gt", "le", "al",
]

# ──────────────────────────────────────────────────────────────────────────────
# Cortex-M exception entry/exit overhead
# ──────────────────────────────────────────────────────────────────────────────
EXCEPTION_OVERHEAD = {
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
# Data structures
# ──────────────────────────────────────────────────────────────────────────────

@dataclass
class Instruction:
    address: int
    raw_bytes: str
    mnemonic: str
    operands: str
    size: int
    min_cycles: int = 1
    max_cycles: int = 1


@dataclass
class Function:
    name: str
    demangled: str
    start_addr: int
    end_addr: int = 0
    instructions: list = field(default_factory=list)
    total_min_cycles: int = 0
    total_max_cycles: int = 0
    instruction_count: int = 0
    code_size: int = 0
    calls: list = field(default_factory=list)
    fpu_ops: int = 0
    load_store_ops: int = 0
    branch_ops: int = 0
    alu_ops: int = 0
    mul_div_ops: int = 0


@dataclass
class PathStage:
    """One stage in the configured critical path."""
    label: str
    functions: list[str] = field(default_factory=list)   # resolved demangled names
    min_cycles: int = 0
    max_cycles: int = 0
    instruction_count: int = 0
    code_size: int = 0
    fpu_ops: int = 0
    is_overhead: bool = False    # True for ISR entry/exit (no disasm)


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


# ──────────────────────────────────────────────────────────────────────────────
# Disassembly parser
# ──────────────────────────────────────────────────────────────────────────────

def parse_disassembly(objdump_output: str) -> list[Function]:
    functions: list[Function] = []
    current_fn: Optional[Function] = None

    fn_pattern = re.compile(r"^([0-9a-fA-F]+)\s+<(.+?)>:$")
    instr_pattern = re.compile(
        r"^\s*([0-9a-fA-F]+):\s+([0-9a-fA-F ]+?)\s{2,}(\S+)\s*(.*?)$"
    )
    call_target = re.compile(r"<(.+?)>")

    for line in objdump_output.splitlines():
        fn_match = fn_pattern.match(line)
        if fn_match:
            if current_fn:
                _finalize(current_fn)
                functions.append(current_fn)
            addr = int(fn_match.group(1), 16)
            name = fn_match.group(2)
            current_fn = Function(name=name, demangled=name, start_addr=addr)
            continue

        if current_fn is None:
            continue

        instr_match = instr_pattern.match(line)
        if instr_match:
            addr = int(instr_match.group(1), 16)
            raw_bytes = instr_match.group(2).strip()
            mnemonic = instr_match.group(3).strip()
            operands = instr_match.group(4).strip()
            byte_count = len(raw_bytes.replace(" ", "")) // 2
            min_c, max_c = get_timing(mnemonic)

            instr = Instruction(
                address=addr, raw_bytes=raw_bytes, mnemonic=mnemonic,
                operands=operands, size=byte_count,
                min_cycles=min_c, max_cycles=max_c,
            )
            current_fn.instructions.append(instr)

            if mnemonic.lower() in ("bl", "blx", "b", "bx") or mnemonic.lower().startswith("bl"):
                ct = call_target.search(operands)
                if ct:
                    current_fn.calls.append(ct.group(1))

    if current_fn:
        _finalize(current_fn)
        functions.append(current_fn)

    return functions


def _finalize(fn: Function):
    fn.instruction_count = len(fn.instructions)
    fn.total_min_cycles = sum(i.min_cycles for i in fn.instructions)
    fn.total_max_cycles = sum(i.max_cycles for i in fn.instructions)
    fn.code_size = sum(i.size for i in fn.instructions)
    if fn.instructions:
        fn.end_addr = fn.instructions[-1].address + fn.instructions[-1].size
    for instr in fn.instructions:
        cat = classify_instruction(instr.mnemonic)
        if cat == "fpu":
            fn.fpu_ops += 1
        elif cat == "load_store":
            fn.load_store_ops += 1
        elif cat == "branch":
            fn.branch_ops += 1
        elif cat == "mul_div":
            fn.mul_div_ops += 1
        else:
            fn.alu_ops += 1


# ──────────────────────────────────────────────────────────────────────────────
# Call-graph walker  (counts call frequency)
# ──────────────────────────────────────────────────────────────────────────────

def _resolve_fn(name: str, lookup: dict[str, Function]) -> Optional[Function]:
    fn = lookup.get(name)
    if fn is not None:
        return fn
    for k, f in lookup.items():
        if name in k:
            return f
    return None


def walk_call_graph(entry_fn: Function, lookup: dict[str, Function]) -> dict[str, dict]:
    """Walk the call graph from *entry_fn*, counting each callee per invocation."""
    breakdown: dict[str, dict] = {}

    def _walk(fn_name: str, visited_stack: frozenset[str]):
        fn = _resolve_fn(fn_name, lookup)
        if fn is None:
            return
        canonical = fn.demangled
        if canonical in visited_stack:
            return
        visited_stack = visited_stack | {canonical}

        if canonical not in breakdown:
            breakdown[canonical] = dict(
                min=0, max=0, instructions=fn.instruction_count,
                code_size=fn.code_size, fpu_ops=fn.fpu_ops,
                load_store=fn.load_store_ops, branch=fn.branch_ops,
                alu=fn.alu_ops, mul_div=fn.mul_div_ops,
                call_count=0,
            )

        breakdown[canonical]["call_count"] += 1
        breakdown[canonical]["min"] += fn.total_min_cycles
        breakdown[canonical]["max"] += fn.total_max_cycles

        for callee in fn.calls:
            _walk(callee, visited_stack)

    _walk(entry_fn.demangled, frozenset())
    return breakdown


# ──────────────────────────────────────────────────────────────────────────────
# Stage analysis  — maps config stages to disassembled functions
# ──────────────────────────────────────────────────────────────────────────────

class StageMatchError(RuntimeError):
    """Raised when a path stage matches no functions in the binary."""


class ConfigError(RuntimeError):
    """Raised when the configuration file is invalid."""


def _validate_config(config: dict) -> None:
    """Validate required config fields and value ranges."""
    errors: list[str] = []

    for field in ("path_stages",):
        if field not in config:
            errors.append(f"Missing required field '{field}'")

    if "path_stages" in config:
        if not isinstance(config["path_stages"], list) or len(config["path_stages"]) == 0:
            errors.append("'path_stages' must be a non-empty list")
        else:
            for i, stage in enumerate(config["path_stages"]):
                if "label" not in stage:
                    errors.append(f"path_stages[{i}]: missing 'label'")
                stage_type = stage.get("type")
                if stage_type is None and not stage.get("patterns"):
                    errors.append(
                        f"path_stages[{i}] ('{stage.get('label', '?')}'): "
                        f"must have 'type' or 'patterns'"
                    )
                # Validate regex patterns are compilable
                for j, pat in enumerate(stage.get("patterns", [])):
                    try:
                        re.compile(pat)
                    except re.error as exc:
                        errors.append(
                            f"path_stages[{i}] ('{stage.get('label', '?')}'): "
                            f"pattern[{j}] '{pat}' is not valid regex: {exc}"
                        )



    if "loop_rates_khz" in config:
        for rate in config["loop_rates_khz"]:
            if not isinstance(rate, (int, float)) or rate <= 0:
                errors.append(f"'loop_rates_khz' contains invalid rate: {rate}")

    if errors:
        raise ConfigError(
            "Configuration validation failed:\n" +
            "\n".join(f"  • {e}" for e in errors)
        )


def analyze_stages(
    config: dict,
    all_fns: list[Function],
    lookup: dict[str, Function],
    *,
    strict: bool = True,
) -> list[PathStage]:
    cortex = config.get("cortex", "m4")
    overhead = EXCEPTION_OVERHEAD.get(cortex, EXCEPTION_OVERHEAD["m4"])
    stages: list[PathStage] = []
    errors: list[str] = []

    for stage_cfg in config["path_stages"]:
        stage_type = stage_cfg.get("type")
        required = stage_cfg.get("required", True)

        if stage_type == "exception_entry":
            stages.append(PathStage(
                label=stage_cfg["label"],
                min_cycles=overhead["entry_min"],
                max_cycles=overhead["entry_max"],
                is_overhead=True,
            ))
            continue

        if stage_type == "exception_exit":
            stages.append(PathStage(
                label=stage_cfg["label"],
                min_cycles=overhead["exit_min"],
                max_cycles=overhead["exit_max"],
                is_overhead=True,
            ))
            continue

        # Pattern-matched stage
        patterns = stage_cfg.get("patterns", [])
        entry_pattern = stage_cfg.get("entry")

        matched: dict[str, Function] = {}
        for fn in all_fns:
            for pat in patterns:
                try:
                    if re.search(pat, fn.demangled):
                        matched[fn.demangled] = fn
                        break
                except re.error as exc:
                    msg = (f"Stage '{stage_cfg['label']}': invalid regex "
                           f"pattern '{pat}': {exc}")
                    errors.append(msg)
                    log(f"  ERROR: {msg}")
                    break

        stage = PathStage(label=stage_cfg["label"])

        if entry_pattern:
            # Walk the call graph from the entry function
            entry_fn = None
            for name, fn in matched.items():
                if re.search(entry_pattern, name):
                    entry_fn = fn
                    break
            if entry_fn is not None:
                breakdown = walk_call_graph(entry_fn, lookup)
                stage.min_cycles = sum(v["min"] for v in breakdown.values())
                stage.max_cycles = sum(v["max"] for v in breakdown.values())
                stage.instruction_count = sum(v["instructions"] for v in breakdown.values())
                stage.code_size = sum(v["code_size"] for v in breakdown.values())
                stage.fpu_ops = sum(v["fpu_ops"] for v in breakdown.values())
                stage.functions = sorted(breakdown.keys())
            else:
                msg = (f"Stage '{stage_cfg['label']}': entry function matching "
                       f"'{entry_pattern}' not found in binary")
                if required:
                    errors.append(msg)
                log(f"  ERROR: {msg}")
        else:
            if not matched and required:
                msg = (f"Stage '{stage_cfg['label']}': no functions matched any "
                       f"of the {len(patterns)} configured patterns")
                errors.append(msg)
                log(f"  ERROR: {msg}")
            # Sum matched functions directly (each counted once)
            for name, fn in matched.items():
                stage.min_cycles += fn.total_min_cycles
                stage.max_cycles += fn.total_max_cycles
                stage.instruction_count += fn.instruction_count
                stage.code_size += fn.code_size
                stage.fpu_ops += fn.fpu_ops
            stage.functions = sorted(matched.keys())

        stages.append(stage)

    if strict and errors:
        log("")
        log("Stage matching failed — the configuration does not align with the binary.")
        log("Unmatched stages:")
        for e in errors:
            log(f"  • {e}")
        log("")
        log("Ensure the path_stages patterns in your config JSON match the ")
        log("demangled symbol names in the ELF. You can inspect them with:")
        log("  arm-none-eabi-objdump -d -C <elf> | grep '^[0-9a-f].*<.*>:$'")
        raise StageMatchError(
            f"{len(errors)} path stage(s) matched no functions in the binary. "
            f"See stderr for details."
        )

    return stages


# ──────────────────────────────────────────────────────────────────────────────
# Report generation
# ──────────────────────────────────────────────────────────────────────────────

def generate_report(
    config: dict,
    stages: list[PathStage],
    all_fns: list[Function],
    lookup: dict[str, Function],
) -> str:
    target = config["target"]
    build_config = config["build_config"]
    clock = config["clock_mhz"]
    loop_rates = config.get("loop_rates_khz", [10, 20, 40])

    total_min = sum(s.min_cycles for s in stages)
    total_max = sum(s.max_cycles for s in stages)
    total_instructions = sum(s.instruction_count for s in stages)
    total_code_size = sum(s.code_size for s in stages)
    total_fpu = sum(s.fpu_ops for s in stages)

    lines: list[str] = []
    path_name = config.get("path_name", "Critical Path")
    lines.append("# Cycle Estimation Report")
    lines.append("")
    lines.append(f"**Target**: `{target}` | **Build**: `{build_config}` | **Clock**: {clock} MHz")
    lines.append(f"**Path**: {path_name}")
    lines.append("")

    # ── Executive Summary ─────────────────────────────────────────────────
    lines.append("## Executive Summary")
    lines.append("")
    lines.append("| Metric | Value |")
    lines.append("|--------|-------|")
    lines.append(f"| Estimated Cycles (min) | **{total_min}** |")
    lines.append(f"| Estimated Cycles (max) | **{total_max}** |")
    lines.append(f"| Total Instructions | {total_instructions} |")
    lines.append(f"| Total Code Size | {total_code_size} bytes |")
    lines.append(f"| FPU Operations | {total_fpu} |")
    lines.append(f"| Path Stages | {len(stages)} |")
    lines.append("")

    # ── CPU Utilization Budget ────────────────────────────────────────────
    lines.append("## CPU Utilization Budget")
    lines.append("")
    lines.append("| Loop Rate | Available Cycles | Min Usage | Max Usage | Status |")
    lines.append("|-----------|-----------------|-----------|-----------|--------|")

    for rate in loop_rates:
        avail = clock * 1_000_000 // (rate * 1_000)
        pct_min = total_min / avail * 100
        pct_max = total_max / avail * 100
        if pct_max > 100:
            status = "🔴 OVER"
        elif pct_max > 50:
            status = "🟡 WARN"
        else:
            status = "🟢 OK"
        lines.append(
            f"| {rate} kHz | {avail} | {total_min} ({pct_min:.1f}%) "
            f"| {total_max} ({pct_max:.1f}%) | {status} |"
        )
    lines.append("")

    # ── Path Breakdown ────────────────────────────────────────────────────
    lines.append("## Path Breakdown")
    lines.append("")
    lines.append("```")
    max_label = max(len(s.label) for s in stages)
    for i, stage in enumerate(stages):
        bar_len = max(1, stage.max_cycles * 40 // max(total_max, 1))
        bar = "█" * bar_len + "░" * (40 - bar_len)
        pct = stage.max_cycles / max(total_max, 1) * 100
        arrow = " → " if i < len(stages) - 1 else "   "
        lines.append(
            f"  {stage.label:<{max_label}}  {bar}  "
            f"{stage.min_cycles:>4d}–{stage.max_cycles:<4d} cycles ({pct:5.1f}%)"
        )
        if i < len(stages) - 1:
            lines.append(f"  {'':>{max_label}}  │")
    lines.append("```")
    lines.append("")

    lines.append("| Stage | Min Cycles | Max Cycles | Instructions | Code Size | FPU Ops | Functions |")
    lines.append("|-------|-----------|-----------|-------------|-----------|---------|-----------|")
    for stage in stages:
        fn_count = len(stage.functions)
        overhead_note = " *(hw overhead)*" if stage.is_overhead else ""
        lines.append(
            f"| {stage.label}{overhead_note} | {stage.min_cycles} | {stage.max_cycles} "
            f"| {stage.instruction_count} | {stage.code_size} B | {stage.fpu_ops} | {fn_count} |"
        )
    lines.append(f"| **TOTAL** | **{total_min}** | **{total_max}** "
                 f"| **{total_instructions}** | **{total_code_size} B** | **{total_fpu}** | |")
    lines.append("")

    # ── Per-Function Detail (for stages with an entry point) ──────────────
    for scfg in config["path_stages"]:
        if not scfg.get("entry"):
            continue
        entry_pattern = scfg["entry"]
        entry_fn = None
        for fn in all_fns:
            if re.search(entry_pattern, fn.demangled):
                entry_fn = fn
                break
        if entry_fn:
            breakdown = walk_call_graph(entry_fn, lookup)
            lines.append(f"## Per-Function Detail — {scfg['label']}")
            lines.append("")
            lines.append("| Function | Calls | Min Cycles | Max Cycles | FPU | Load/Store | Branch | ALU |")
            lines.append("|----------|-------|-----------|-----------|-----|------------|--------|-----|")

            sorted_fns = sorted(breakdown.items(), key=lambda x: x[1]["max"], reverse=True)
            for fn_name, stats in sorted_fns:
                short = fn_name[:60] + "..." if len(fn_name) > 60 else fn_name
                lines.append(
                    f"| `{short}` | {stats['call_count']}x "
                    f"| {stats['min']} | {stats['max']} "
                    f"| {stats['fpu_ops']} | {stats['load_store']} "
                    f"| {stats['branch']} | {stats['alu']} |"
                )
            lines.append("")

    # ── Instruction Mix ───────────────────────────────────────────────────
    lines.append("## Instruction Mix")
    lines.append("")
    # Collect from all matched functions across stages
    mix: dict[str, int] = {"FPU": 0, "Load/Store": 0, "Branch": 0, "ALU": 0, "Mul/Div": 0}
    for stage in stages:
        for fn_name in stage.functions:
            fn = lookup.get(fn_name)
            if fn:
                mix["FPU"] += fn.fpu_ops
                mix["Load/Store"] += fn.load_store_ops
                mix["Branch"] += fn.branch_ops
                mix["ALU"] += fn.alu_ops
                mix["Mul/Div"] += fn.mul_div_ops
    total_instrs = sum(mix.values()) or 1

    lines.append("```")
    for label, count in mix.items():
        pct = count / total_instrs * 100
        bar = "█" * int(pct / 2) + "░" * (50 - int(pct / 2))
        lines.append(f"  {label:12s} {bar} {count:4d} ({pct:5.1f}%)")
    lines.append("```")
    lines.append("")

    # ── Optimization Indicators ───────────────────────────────────────────
    lines.append("## Optimization Quality Indicators")
    lines.append("")

    fpu_ratio = mix["FPU"] / total_instrs
    ls_ratio = mix["Load/Store"] / total_instrs

    if fpu_ratio > 0.3:
        lines.append(f"- ✅ **High FPU utilization** ({fpu_ratio:.0%}) — good use of hardware FPU")
    elif fpu_ratio > 0.15:
        lines.append(f"- ⚠️ **Moderate FPU utilization** ({fpu_ratio:.0%}) — some overhead from non-FPU ops")
    else:
        lines.append(f"- ❌ **Low FPU utilization** ({fpu_ratio:.0%}) — excessive overhead")

    if ls_ratio > 0.4:
        lines.append(f"- ❌ **High load/store ratio** ({ls_ratio:.0%}) — register spilling or poor data locality")
    elif ls_ratio > 0.25:
        lines.append(f"- ⚠️ **Moderate load/store ratio** ({ls_ratio:.0%})")
    else:
        lines.append(f"- ✅ **Low load/store ratio** ({ls_ratio:.0%}) — efficient register usage")

    # Check for FMA
    has_vfma = False
    has_vsqrt_vdiv = False
    for fn in all_fns:
        for instr in fn.instructions:
            ml = instr.mnemonic.lower()
            if ml.startswith("vfma"):
                has_vfma = True
            if ml in ("vsqrt.f32", "vdiv.f32"):
                has_vsqrt_vdiv = True
        if has_vfma and has_vsqrt_vdiv:
            break

    if has_vfma:
        lines.append("- ✅ **FMA instructions detected** — fused multiply-add in use")
    else:
        lines.append("- ⚠️ **No FMA instructions** — consider `-ffast-math` or VFMA intrinsics")

    if has_vsqrt_vdiv:
        lines.append("- ⚠️ **vsqrt/vdiv detected** — 14 cycles each; consider LUT approximations")
    lines.append("")

    return "\n".join(lines)


def generate_pr_comment(config: dict, stages: list[PathStage]) -> str:
    """Generate a concise PR comment banner with Executive Summary + CPU Budget."""
    target = config["target"]
    build_config = config["build_config"]
    clock = config["clock_mhz"]
    loop_rates = config.get("loop_rates_khz", [10, 20, 40])

    total_min = sum(s.min_cycles for s in stages)
    total_max = sum(s.max_cycles for s in stages)
    total_instructions = sum(s.instruction_count for s in stages)
    total_code_size = sum(s.code_size for s in stages)

    lines: list[str] = []
    path_name = config.get("path_name", "Critical Path")
    lines.append("## 🏎️ Cycle Estimation")
    lines.append("")
    lines.append(f"**Target**: `{target}` | **Build**: `{build_config}` | **Clock**: {clock} MHz")
    lines.append(f"**Path**: {path_name}")
    lines.append("")

    # ── Executive Summary table ───────────────────────────────────────────
    lines.append("### Executive Summary")
    lines.append("")
    lines.append("| Metric | Value |")
    lines.append("|--------|-------|")
    lines.append(f"| Estimated Cycles (min — max) | **{total_min}** — **{total_max}** |")
    lines.append(f"| Total Instructions | {total_instructions} |")
    lines.append(f"| Code Size | {total_code_size} bytes |")
    lines.append(f"| Path Stages | {len(stages)} |")
    lines.append("")

    # ── CPU Utilization Budget ────────────────────────────────────────────
    lines.append("### CPU Utilization Budget")
    lines.append("")
    lines.append("| Loop Rate | Available Cycles | Min Usage | Max Usage | Status |")
    lines.append("|-----------|-----------------|-----------|-----------|--------|")

    for rate in loop_rates:
        avail = clock * 1_000_000 // (rate * 1_000)
        pct_min = total_min / avail * 100
        pct_max = total_max / avail * 100
        if pct_max > 100:
            status = "🔴 OVER"
        elif pct_max > 50:
            status = "🟡 WARN"
        else:
            status = "🟢 OK"
        lines.append(
            f"| {rate} kHz | {avail} | {total_min} ({pct_min:.1f}%) "
            f"| {total_max} ({pct_max:.1f}%) | {status} |"
        )
    lines.append("")

    # ── Path timeline ─────────────────────────────────────────────────────
    lines.append("### Path Breakdown")
    lines.append("")
    lines.append("| Stage | Cycles | % |")
    lines.append("|-------|--------|---|")
    for i, stage in enumerate(stages):
        pct = stage.max_cycles / max(total_max, 1) * 100
        lines.append(f"| {stage.label} | {stage.min_cycles}–{stage.max_cycles} | {pct:.0f}% |")
    lines.append(f"| **Total** | **{total_min}–{total_max}** | |")
    lines.append("")

    lines.append("> 📦 Full report, annotated disassembly and JSON metrics available in workflow artifacts.")

    return "\n".join(lines)


def generate_annotated_disasm(
    config: dict,
    all_fns: list[Function],
    stages: list[PathStage],
) -> str:
    """Generate annotated disassembly for all functions in the path."""
    lines: list[str] = []
    path_name = config.get("path_name", "Critical Path")
    lines.append(f"# Annotated Disassembly — {path_name}")
    lines.append("")

    seen: set[str] = set()
    for stage in stages:
        if stage.is_overhead:
            continue
        lines.append(f"## Stage: {stage.label}")
        lines.append("")

        for fn_name in sorted(stage.functions):
            if fn_name in seen:
                continue
            seen.add(fn_name)
            fn = None
            for f in all_fns:
                if f.demangled == fn_name:
                    fn = f
                    break
            if fn is None:
                continue

            short = fn_name[:80] + "..." if len(fn_name) > 80 else fn_name
            lines.append(f"### `{short}`")
            lines.append("")
            lines.append(
                f"Address: `0x{fn.start_addr:08x}`–`0x{fn.end_addr:08x}` "
                f"| {fn.code_size} bytes | {fn.instruction_count} instructions "
                f"| {fn.total_min_cycles}–{fn.total_max_cycles} cycles"
            )
            lines.append("")
            lines.append("```asm")
            lines.append(f"; {'Addr':>10s}  {'Bytes':<14s}  {'Mn':>3s} {'Mx':>3s}  Instruction")
            lines.append(f"; {'-'*10}  {'-'*14}  {'-'*3} {'-'*3}  {'-'*40}")

            for instr in fn.instructions:
                cat = classify_instruction(instr.mnemonic)
                marker = {"fpu": "F", "load_store": "M", "branch": "B",
                          "mul_div": "X"}.get(cat, " ")
                lines.append(
                    f"  0x{instr.address:08x}  {instr.raw_bytes:<14s}  "
                    f"{instr.min_cycles:>3d} {instr.max_cycles:>3d} {marker} "
                    f"{instr.mnemonic}\t{instr.operands}"
                )
            lines.append("```")
            lines.append("")

            if fn.calls:
                lines.append("**Calls:**")
                for callee in fn.calls:
                    short_callee = callee[:70] + "..." if len(callee) > 70 else callee
                    lines.append(f"  - `{short_callee}`")
                lines.append("")

    return "\n".join(lines)


def generate_json_metrics(
    config: dict, stages: list[PathStage],
) -> dict:
    total_min = sum(s.min_cycles for s in stages)
    total_max = sum(s.max_cycles for s in stages)
    total_instr = sum(s.instruction_count for s in stages)
    total_code_size = sum(s.code_size for s in stages)
    clock = config["clock_mhz"]

    return {
        "target": config["target"],
        "build_config": config["build_config"],
        "clock_mhz": clock,
        "cortex": config.get("cortex", "m4"),
        "path": config.get("path_name", "Critical Path"),
        "summary": {
            "estimated_min_cycles": total_min,
            "estimated_max_cycles": total_max,
            "total_instructions": total_instr,
            "total_code_size_bytes": total_code_size,
            "path_stages": len(stages),
        },
        "utilization": {
            f"{rate}khz_min_pct": round(total_min / (clock * 1_000_000 // (rate * 1_000)) * 100, 2)
            for rate in config.get("loop_rates_khz", [20])
        } | {
            f"{rate}khz_max_pct": round(total_max / (clock * 1_000_000 // (rate * 1_000)) * 100, 2)
            for rate in config.get("loop_rates_khz", [20])
        },
        "stages": [
            {
                "label": s.label,
                "min_cycles": s.min_cycles,
                "max_cycles": s.max_cycles,
                "instructions": s.instruction_count,
                "code_size_bytes": s.code_size,
                "fpu_ops": s.fpu_ops,
                "is_overhead": s.is_overhead,
                "functions": s.functions,
            }
            for s in stages
        ],
    }


# ──────────────────────────────────────────────────────────────────────────────
# Tooling helpers
# ──────────────────────────────────────────────────────────────────────────────

def log(msg: str):
    print(msg, file=sys.stderr)


def run_objdump(elf: str, tool: str = "arm-none-eabi-objdump") -> str:
    cmd = [tool, "-d", "-C", elf]
    log(f"Running: {' '.join(cmd)}")
    return subprocess.run(cmd, capture_output=True, text=True, check=True).stdout


def run_size(elf: str, tool: str = "arm-none-eabi-size") -> str:
    cmd = [tool, "-A", elf]
    try:
        return subprocess.run(cmd, capture_output=True, text=True, check=True).stdout
    except (subprocess.CalledProcessError, FileNotFoundError):
        return ""


# ──────────────────────────────────────────────────────────────────────────────
# main
# ──────────────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="ARM Cortex-M cycle estimation for control loops"
    )
    parser.add_argument("config", help="JSON configuration file")
    parser.add_argument("--elf", required=True, help="Path to ELF binary")
    parser.add_argument("--output-dir", default=".", help="Output directory")
    parser.add_argument("--objdump", default="arm-none-eabi-objdump")
    parser.add_argument("--size-tool", default="arm-none-eabi-size")
    parser.add_argument("--disasm-file", help="Pre-generated disassembly file")
    parser.add_argument(
        "--no-strict", action="store_true",
        help="Do not fail when path stages match no functions",
    )
    parser.add_argument(
        "--target", required=True,
        help="Target board identifier (e.g. EK-TM4C1294XL)",
    )
    parser.add_argument(
        "--build-config", required=True,
        help="Build configuration (e.g. RelWithDebInfo, Debug)",
    )
    parser.add_argument(
        "--clock-mhz", required=True, type=int,
        help="CPU clock frequency in MHz (e.g. 120)",
    )
    parser.add_argument(
        "--cortex", default="m4",
        help="Cortex-M variant for timing model (default: m4)",
    )
    args = parser.parse_args()

    # Load config
    config_path = args.config
    elf_path = args.elf

    if not os.path.isfile(config_path):
        log(f"ERROR: Config file not found: {config_path}")
        sys.exit(1)

    if not os.path.isfile(elf_path):
        log(f"ERROR: ELF file not found: {elf_path}")
        sys.exit(1)

    try:
        with open(config_path) as f:
            config = json.load(f)
    except json.JSONDecodeError as exc:
        log(f"ERROR: Failed to parse config JSON: {exc}")
        sys.exit(1)

    # Merge hardware parameters from CLI into config
    config["target"] = args.target
    config["build_config"] = args.build_config
    config["clock_mhz"] = args.clock_mhz
    config["cortex"] = args.cortex

    # Validate hardware parameters
    if config["clock_mhz"] <= 0:
        log(f"ERROR: --clock-mhz must be a positive integer, got {config['clock_mhz']}")
        sys.exit(1)
    if config["cortex"] not in EXCEPTION_OVERHEAD:
        valid = ", ".join(sorted(EXCEPTION_OVERHEAD))
        log(f"ERROR: --cortex '{config['cortex']}' is not supported. Valid values: {valid}")
        sys.exit(1)

    try:
        _validate_config(config)
    except ConfigError as exc:
        log(f"ERROR: {exc}")
        sys.exit(1)

    os.makedirs(args.output_dir, exist_ok=True)

    # Disassembly
    if args.disasm_file:
        if not os.path.isfile(args.disasm_file):
            log(f"ERROR: Disassembly file not found: {args.disasm_file}")
            sys.exit(1)
        with open(args.disasm_file) as f:
            disasm = f.read()
    else:
        try:
            disasm = run_objdump(elf_path, args.objdump)
        except FileNotFoundError:
            log(f"ERROR: objdump tool not found: '{args.objdump}'")
            log("Install the ARM toolchain or pass --objdump with the correct path.")
            sys.exit(1)
        except subprocess.CalledProcessError as exc:
            log(f"ERROR: objdump failed (exit {exc.returncode}): {exc.stderr}")
            sys.exit(1)

    disasm_path = os.path.join(args.output_dir, "full_disassembly.txt")
    with open(disasm_path, "w") as f:
        f.write(disasm)
    log(f"Full disassembly: {disasm_path}")

    # Size report
    size_out = run_size(elf_path, args.size_tool)
    if size_out:
        with open(os.path.join(args.output_dir, "size_report.txt"), "w") as f:
            f.write(size_out)

    # Parse
    all_fns = parse_disassembly(disasm)
    lookup = {fn.demangled: fn for fn in all_fns}
    log(f"Parsed {len(all_fns)} functions")

    if len(all_fns) == 0:
        log("ERROR: No functions found in the disassembly.")
        log("The ELF may be stripped, empty, or not an ARM binary.")
        sys.exit(1)

    # Analyze stages
    stages = analyze_stages(config, all_fns, lookup, strict=not args.no_strict)
    log("Path stages:")
    for s in stages:
        log(f"  {s.label}: {s.min_cycles}–{s.max_cycles} cycles, "
            f"{s.instruction_count} instrs, {len(s.functions)} fns")

    total_min = sum(s.min_cycles for s in stages)
    total_max = sum(s.max_cycles for s in stages)

    # Generate reports
    report = generate_report(config, stages, all_fns, lookup)
    with open(os.path.join(args.output_dir, "cycle_estimation_report.md"), "w") as f:
        f.write(report)

    pr_comment = generate_pr_comment(config, stages)
    with open(os.path.join(args.output_dir, "pr_comment.md"), "w") as f:
        f.write(pr_comment)

    annotated = generate_annotated_disasm(config, all_fns, stages)
    with open(os.path.join(args.output_dir, "annotated_disassembly.md"), "w") as f:
        f.write(annotated)

    metrics = generate_json_metrics(config, stages)
    with open(os.path.join(args.output_dir, "cycle_metrics.json"), "w") as f:
        json.dump(metrics, f, indent=2)

    # Summary to stdout
    clock = config["clock_mhz"]
    avail_20k = clock * 1_000_000 // 20_000
    print(f"\n{'='*65}")
    path_name = config.get("path_name", "Critical Path")
    print(f" Cycle Estimation ({config['target']} {config['build_config']})")
    print(f"{'='*65}")
    print(f" Path: {path_name}")
    print(f" Estimated cycles: {total_min} — {total_max}")
    print(f" Budget @ 20 kHz / {clock} MHz: {avail_20k} cycles "
          f"({total_max / avail_20k * 100:.1f}% max)")
    print(f" Stages: {len(stages)}")
    for s in stages:
        pct = s.max_cycles / max(total_max, 1) * 100
        print(f"   {s.label:<40s}  {s.min_cycles:>4d}–{s.max_cycles:<4d}  ({pct:.0f}%)")
    print(f"{'='*65}")


if __name__ == "__main__":
    main()
