"""
CLI entry point.  Invoked as:  python3 -m cycle_analysis  <args>
"""

from __future__ import annotations

import argparse
import json
import os
import sys

from cycle_analysis.analysis import _validate_config, analyze_stages
from cycle_analysis.models import ConfigError, StageMatchError
from cycle_analysis.parser import parse_disassembly
from cycle_analysis.reports import (
    generate_annotated_disasm,
    generate_cpu_budget_table,
    generate_json_metrics,
    generate_pr_comment,
    generate_report,
)
from cycle_analysis.timing_model import EXCEPTION_OVERHEAD
from cycle_analysis.tooling import log, run_objdump, run_size


def main() -> None:
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

    config["target"] = args.target
    config["build_config"] = args.build_config
    config["clock_mhz"] = args.clock_mhz
    config["cortex"] = args.cortex

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
        except Exception as exc:
            log(f"ERROR: objdump failed: {exc}")
            sys.exit(1)

    disasm_path = os.path.join(args.output_dir, "full_disassembly.txt")
    with open(disasm_path, "w") as f:
        f.write(disasm)
    log(f"Full disassembly: {disasm_path}")

    size_out = run_size(elf_path, args.size_tool)
    if size_out:
        with open(os.path.join(args.output_dir, "size_report.txt"), "w") as f:
            f.write(size_out)

    all_fns = parse_disassembly(disasm)
    lookup = {fn.demangled: fn for fn in all_fns}
    log(f"Parsed {len(all_fns)} functions")

    if len(all_fns) == 0:
        log("ERROR: No functions found in the disassembly.")
        log("The ELF may be stripped, empty, or not an ARM binary.")
        sys.exit(1)

    try:
        stages = analyze_stages(config, all_fns, lookup, strict=not args.no_strict)
    except StageMatchError as exc:
        log(f"ERROR: {exc}")
        sys.exit(1)

    log("Path stages:")
    for s in stages:
        log(f"  {s.label}: {s.min_cycles}–{s.max_cycles} cycles, "
            f"{s.instruction_count} instrs, {len(s.functions)} fns")

    total_min = sum(s.min_cycles for s in stages)
    total_max = sum(s.max_cycles for s in stages)

    report = generate_report(config, stages, all_fns, lookup)
    with open(os.path.join(args.output_dir, "cycle_estimation_report.md"), "w") as f:
        f.write(report)

    budget = generate_cpu_budget_table(config, stages)
    with open(os.path.join(args.output_dir, "cpu_budget.md"), "w") as f:
        f.write(budget)

    pr_comment = generate_pr_comment(config, stages)
    with open(os.path.join(args.output_dir, "pr_comment.md"), "w") as f:
        f.write(pr_comment)

    annotated = generate_annotated_disasm(config, all_fns, stages)
    with open(os.path.join(args.output_dir, "annotated_disassembly.md"), "w") as f:
        f.write(annotated)

    metrics = generate_json_metrics(config, stages)
    with open(os.path.join(args.output_dir, "cycle_metrics.json"), "w") as f:
        json.dump(metrics, f, indent=2)

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
