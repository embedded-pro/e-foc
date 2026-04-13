"""
Report generators: Markdown, PR comment, annotated disassembly, JSON metrics,
and cpu-budget table.
"""

from __future__ import annotations

import re

from cycle_analysis.analysis import walk_call_graph
from cycle_analysis.models import Function, PathStage
from cycle_analysis.timing_model import classify_instruction


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

    # ── Per-Function Detail ───────────────────────────────────────────────
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


def generate_cpu_budget_table(config: dict, stages: list[PathStage]) -> str:
    """Generate only the CPU utilization budget markdown table rows (no headings)."""
    clock = config["clock_mhz"]
    loop_rates = config.get("loop_rates_khz", [10, 20, 40])

    total_min = sum(s.min_cycles for s in stages)
    total_max = sum(s.max_cycles for s in stages)

    lines: list[str] = []
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

    lines.append("### Executive Summary")
    lines.append("")
    lines.append("| Metric | Value |")
    lines.append("|--------|-------|")
    lines.append(f"| Estimated Cycles (min — max) | **{total_min}** — **{total_max}** |")
    lines.append(f"| Total Instructions | {total_instructions} |")
    lines.append(f"| Code Size | {total_code_size} bytes |")
    lines.append(f"| Path Stages | {len(stages)} |")
    lines.append("")

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

    lines.append("### Path Breakdown")
    lines.append("")
    lines.append("| Stage | Cycles | % |")
    lines.append("|-------|--------|---|")
    for stage in stages:
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
    config: dict,
    stages: list[PathStage],
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
