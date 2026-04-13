#!/usr/bin/env python3
"""
Multi-configuration cycle analysis runner.

Runs analyze_cycles.py analysis for each entry in a JSON configuration array,
then generates a combined PR comment and step summary with per-mode collapsible
sections showing the CPU Utilization Budget by default.

Usage:
    python3 analyze_multi.py analyses.json \\
        --target EK-TM4C1294XL \\
        --build-config RelWithDebInfo \\
        --clock-mhz 120 \\
        --output-dir cycle-analysis-combined

analyses.json format:
    [
      {
        "label": "Torque mode",
        "elf_path": "build/EK-TM4C1294XL/.../torque.elf",
        "config_path": "targets/.../torque/cycle-analysis.json",
        "output_dir": "cycle-analysis-torque"
      },
      ...
    ]
"""

from __future__ import annotations

import argparse
import json
import os
import sys
from typing import Any

from cycle_analysis import (
    ConfigError,
    EXCEPTION_OVERHEAD,
    analyze_stages,
    generate_annotated_disasm,
    generate_cpu_budget_table,
    generate_json_metrics,
    generate_report,
    log,
    parse_disassembly,
    run_objdump,
    run_size,
)
from cycle_analysis.analysis import _validate_config


# ──────────────────────────────────────────────────────────────────────────────
# Single-analysis driver
# ──────────────────────────────────────────────────────────────────────────────

def run_single_analysis(
    entry: dict[str, Any],
    shared_args: argparse.Namespace,
) -> tuple[str, str]:
    """Run analysis for one entry. Returns (cpu_budget_md, full_report_md)."""
    label = entry["label"]
    elf_path = entry["elf_path"]
    config_path = entry["config_path"]
    output_dir = entry["output_dir"]

    log(f"\n--- Analyzing: {label} ---")

    if not os.path.isfile(elf_path):
        log(f"ERROR: ELF not found: {elf_path}")
        sys.exit(1)

    if not os.path.isfile(config_path):
        log(f"ERROR: Config not found: {config_path}")
        sys.exit(1)

    try:
        with open(config_path) as f:
            config = json.load(f)
    except json.JSONDecodeError as exc:
        log(f"ERROR: Failed to parse config JSON '{config_path}': {exc}")
        sys.exit(1)

    # Merge hardware parameters from shared CLI args into per-mode config
    config["target"] = shared_args.target
    config["build_config"] = shared_args.build_config
    config["clock_mhz"] = shared_args.clock_mhz
    config["cortex"] = shared_args.cortex

    try:
        _validate_config(config)
    except ConfigError as exc:
        log(f"ERROR: {exc}")
        sys.exit(1)

    os.makedirs(output_dir, exist_ok=True)

    try:
        disasm = run_objdump(elf_path, shared_args.objdump)
    except FileNotFoundError:
        log(f"ERROR: objdump tool not found: '{shared_args.objdump}'")
        log("Install the ARM toolchain or pass --objdump with the correct path.")
        sys.exit(1)
    except Exception as exc:
        log(f"ERROR: objdump failed: {exc}")
        sys.exit(1)

    with open(os.path.join(output_dir, "full_disassembly.txt"), "w") as f:
        f.write(disasm)

    size_out = run_size(elf_path, shared_args.size_tool)
    if size_out:
        with open(os.path.join(output_dir, "size_report.txt"), "w") as f:
            f.write(size_out)

    all_fns = parse_disassembly(disasm)
    lookup = {fn.demangled: fn for fn in all_fns}
    log(f"Parsed {len(all_fns)} functions")

    if len(all_fns) == 0:
        log("ERROR: No functions found in the disassembly.")
        log("The ELF may be stripped, empty, or not an ARM binary.")
        sys.exit(1)

    stages = analyze_stages(
        config, all_fns, lookup, strict=not getattr(shared_args, "no_strict", False)
    )

    report = generate_report(config, stages, all_fns, lookup)
    with open(os.path.join(output_dir, "cycle_estimation_report.md"), "w") as f:
        f.write(report)

    budget = generate_cpu_budget_table(config, stages)
    with open(os.path.join(output_dir, "cpu_budget.md"), "w") as f:
        f.write(budget)

    annotated = generate_annotated_disasm(config, all_fns, stages)
    with open(os.path.join(output_dir, "annotated_disassembly.md"), "w") as f:
        f.write(annotated)

    metrics = generate_json_metrics(config, stages)
    with open(os.path.join(output_dir, "cycle_metrics.json"), "w") as f:
        json.dump(metrics, f, indent=2)

    total_min = sum(s.min_cycles for s in stages)
    total_max = sum(s.max_cycles for s in stages)
    log(f"{label}: {total_min}–{total_max} cycles, {len(stages)} stages")

    return budget, report


# ──────────────────────────────────────────────────────────────────────────────
# Combined output generators
# ──────────────────────────────────────────────────────────────────────────────

def _build_collapsible_sections(
    analyses: list[dict[str, Any]],
    budgets: list[str],
    reports: list[str],
    target: str,
) -> list[str]:
    """Return lines shared between the PR comment and the step summary."""
    lines: list[str] = []
    for i, (entry, budget, report) in enumerate(zip(analyses, budgets, reports)):
        label = entry["label"]
        lines.append(f"### {label} | {target}")
        lines.append("")
        lines.append("**CPU Utilization Budget**")
        lines.append("")
        lines.append(budget)
        lines.append("")
        lines.append("<details>")
        lines.append("<summary>Click to expand full details</summary>")
        lines.append("")
        lines.append(report)
        lines.append("")
        lines.append("</details>")
        lines.append("")
        if i < len(analyses) - 1:
            lines.append("---")
            lines.append("")
    return lines


def generate_combined_pr_comment(
    analyses: list[dict[str, Any]],
    budgets: list[str],
    reports: list[str],
    target: str,
) -> str:
    lines: list[str] = [
        "<!-- cycle-analysis-comment -->",
        "## 🏎️ Cycle Analysis Results",
        "",
    ]
    lines.extend(_build_collapsible_sections(analyses, budgets, reports, target))
    return "\n".join(lines)


def generate_combined_step_summary(
    analyses: list[dict[str, Any]],
    budgets: list[str],
    reports: list[str],
    target: str,
) -> str:
    lines: list[str] = [
        "## 🏎️ Cycle Analysis Results",
        "",
    ]
    lines.extend(_build_collapsible_sections(analyses, budgets, reports, target))
    return "\n".join(lines)


def generate_combined_metrics(
    analyses: list[dict[str, Any]],
) -> dict:
    """Read per-mode metrics and build a combined metrics dict."""
    modes: list[dict] = []
    overall_max = 0

    for entry in analyses:
        metrics_path = os.path.join(entry["output_dir"], "cycle_metrics.json")
        if not os.path.isfile(metrics_path):
            continue
        with open(metrics_path) as f:
            m = json.load(f)
        m["mode_label"] = entry["label"]
        modes.append(m)
        overall_max = max(overall_max, m["summary"]["estimated_max_cycles"])

    return {
        "modes": modes,
        "overall_max_cycles": overall_max,
    }


# ──────────────────────────────────────────────────────────────────────────────
# main
# ──────────────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Multi-configuration ARM Cortex-M cycle analysis"
    )
    parser.add_argument(
        "analyses_config",
        help="JSON file containing an array of analysis entries",
    )
    parser.add_argument("--target", required=True, help="Target board identifier")
    parser.add_argument("--build-config", required=True, help="Build configuration")
    parser.add_argument("--clock-mhz", required=True, type=int, help="CPU clock in MHz")
    parser.add_argument("--cortex", default="m4", help="Cortex-M variant (default: m4)")
    parser.add_argument("--objdump", default="arm-none-eabi-objdump")
    parser.add_argument("--size-tool", default="arm-none-eabi-size")
    parser.add_argument("--output-dir", default="cycle-analysis-combined")
    parser.add_argument(
        "--no-strict",
        action="store_true",
        help="Do not fail when path stages match no functions",
    )
    args = parser.parse_args()

    if not os.path.isfile(args.analyses_config):
        log(f"ERROR: Analyses config not found: {args.analyses_config}")
        sys.exit(1)

    if args.clock_mhz <= 0:
        log(f"ERROR: --clock-mhz must be a positive integer, got {args.clock_mhz}")
        sys.exit(1)

    if args.cortex not in EXCEPTION_OVERHEAD:
        valid = ", ".join(sorted(EXCEPTION_OVERHEAD))
        log(f"ERROR: --cortex '{args.cortex}' is not supported. Valid values: {valid}")
        sys.exit(1)

    try:
        with open(args.analyses_config) as f:
            analyses = json.load(f)
    except json.JSONDecodeError as exc:
        log(f"ERROR: Failed to parse analyses config JSON: {exc}")
        sys.exit(1)

    if not isinstance(analyses, list) or len(analyses) == 0:
        log("ERROR: Analyses config must be a non-empty JSON array")
        sys.exit(1)

    os.makedirs(args.output_dir, exist_ok=True)

    budgets: list[str] = []
    reports: list[str] = []

    for entry in analyses:
        budget, report = run_single_analysis(entry, args)
        budgets.append(budget)
        reports.append(report)

    comment = generate_combined_pr_comment(analyses, budgets, reports, args.target)
    comment_path = os.path.join(args.output_dir, "combined_pr_comment.md")
    with open(comment_path, "w") as f:
        f.write(comment)
    log(f"\nCombined PR comment: {comment_path}")

    summary = generate_combined_step_summary(analyses, budgets, reports, args.target)
    summary_path = os.path.join(args.output_dir, "combined_step_summary.md")
    with open(summary_path, "w") as f:
        f.write(summary)
    log(f"Combined step summary: {summary_path}")

    combined_metrics = generate_combined_metrics(analyses)
    metrics_path = os.path.join(args.output_dir, "combined_metrics.json")
    with open(metrics_path, "w") as f:
        json.dump(combined_metrics, f, indent=2)
    log(f"Combined metrics: {metrics_path}")

    # Print stdout summary
    clock = args.clock_mhz
    avail_20k = clock * 1_000_000 // 20_000
    print(f"\n{'='*65}")
    print(f" Multi-Config Cycle Analysis ({args.target} {args.build_config})")
    print(f"{'='*65}")
    for entry in analyses:
        mp = os.path.join(entry["output_dir"], "cycle_metrics.json")
        if os.path.isfile(mp):
            with open(mp) as f:
                m = json.load(f)
            total_min = m["summary"]["estimated_min_cycles"]
            total_max = m["summary"]["estimated_max_cycles"]
            pct = total_max / avail_20k * 100 if avail_20k > 0 else 0.0
            print(
                f" {entry['label']:<22s}  {total_min:>4d}–{total_max:<4d} cycles  "
                f"({pct:.1f}% max @ 20 kHz)"
            )
    print(f"{'='*65}")


if __name__ == "__main__":
    main()
