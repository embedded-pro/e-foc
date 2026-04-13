"""
Call-graph walker and stage analysis.

Provides walk_call_graph() for recursive cycle attribution and
analyze_stages() for mapping a JSON config to disassembled functions.
"""

from __future__ import annotations

import re
from typing import Optional

from cycle_analysis.models import (
    ConfigError,
    Function,
    PathStage,
    StageMatchError,
)
from cycle_analysis.timing_model import EXCEPTION_OVERHEAD
from cycle_analysis.tooling import log


# ──────────────────────────────────────────────────────────────────────────────
# Call-graph walker
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

    def _walk(fn_name: str, visited_stack: frozenset[str]) -> None:
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
# Config validation
# ──────────────────────────────────────────────────────────────────────────────

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


# ──────────────────────────────────────────────────────────────────────────────
# Stage analysis
# ──────────────────────────────────────────────────────────────────────────────

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
