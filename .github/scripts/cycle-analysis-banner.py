#!/usr/bin/env python3
"""Compose one PR comment from the cortex-cycle-budget runs of a target.

Each run is passed as LABEL OUTPUT_DIR MAX_CYCLES: the overview table always shows one row per run,
the per-stage breakdowns go into a collapsed section. A run that wrote no cycle_metrics.json is
reported as having produced no result rather than dropped. With --report-only the budgets are shown but not
enforced, so a path over budget is flagged as a warning instead of a failure.
"""

import argparse
import json
import re
from pathlib import Path

RATE_KEY = re.compile(r"^(?P<rate>\d+(?:\.\d+)?)khz_max_pct$")


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", required=True)
    parser.add_argument("--build-config", required=True)
    parser.add_argument("--cortex", required=True)
    parser.add_argument("--clock-mhz", required=True, type=int)
    parser.add_argument("--marker", required=True)
    parser.add_argument("--artifact-prefix", required=True)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--report-only", action="store_true")
    parser.add_argument(
        "--analysis",
        nargs=3,
        action="append",
        required=True,
        metavar=("LABEL", "OUTPUT_DIR", "MAX_CYCLES"),
    )
    return parser.parse_args()


def load_metrics(output_dir):
    path = Path(output_dir) / "cycle_metrics.json"
    if not path.is_file():
        return None
    return json.loads(path.read_text(encoding="utf-8"))


def loop_rate_khz(metrics):
    for key in metrics["utilization"]:
        match = RATE_KEY.match(key)
        if match:
            return float(match.group("rate"))
    raise ValueError(f"no '<rate>khz_max_pct' utilization entry for path '{metrics['path']}'")


def period_cycles(clock_mhz, rate_khz):
    return int(clock_mhz * 1000 / rate_khz)


def overview_row(label, metrics, max_cycles, clock_mhz, report_only):
    if metrics is None:
        return f"| {label} | — | — | {max_cycles} | ⚠️ no result |"
    rate = loop_rate_khz(metrics)
    period = period_cycles(clock_mhz, rate)
    worst = metrics["summary"]["estimated_max_cycles"]
    if worst <= max_cycles:
        status = "✅ within budget"
    else:
        status = "⚠️ over budget (not enforced)" if report_only else "❌ over budget"
    return (
        f"| {label} | {rate:g} kHz | {worst} ({worst / period:.1%}) "
        f"| {max_cycles} ({max_cycles / period:.1%}) | {status} |"
    )


def breakdown(label, metrics):
    if metrics is None:
        return [f"#### {label}", "", "_The analysis wrote no metrics; see the job log._", ""]
    summary = metrics["summary"]
    total_max = max(summary["estimated_max_cycles"], 1)
    lines = [
        f"#### {label}",
        "",
        f"{metrics['path']}",
        "",
        "| Stage | Cycles (min–max) | Share |",
        "|-------|-----------------:|------:|",
    ]
    for stage in metrics["stages"]:
        lines.append(
            f"| {stage['label']} | {stage['min_cycles']}–{stage['max_cycles']} "
            f"| {stage['max_cycles'] / total_max:.0%} |"
        )
    lines.extend(
        (
            f"| **Total** | **{summary['estimated_min_cycles']}–{summary['estimated_max_cycles']}** | |",
            "",
            f"{summary['total_instructions']} instructions · {summary['total_code_size_bytes']} bytes "
            f"· {summary['path_stages']} stages",
            "",
        )
    )
    return lines


def compose(arguments):
    runs = [(label, load_metrics(directory), int(limit)) for label, directory, limit in arguments.analysis]
    lines = [
        arguments.marker,
        f"## 🏎️ Cycle Estimation — {arguments.target}",
        "",
        f"`{arguments.build_config}` · Cortex-{arguments.cortex.upper()} · {arguments.clock_mhz} MHz "
        "· percentages are of the loop period",
        "",
    ]
    if arguments.report_only:
        lines.extend(("> ℹ️ Report only: the budgets below are shown for comparison and do not fail the build.", ""))
    lines.extend(
        (
            "| Path | Rate | Max cycles | Budget | Status |",
            "|------|------|-----------:|-------:|--------|",
        )
    )
    lines.extend(
        overview_row(label, metrics, limit, arguments.clock_mhz, arguments.report_only) for label, metrics, limit in runs
    )
    lines.extend(("", "<details>", "<summary>Path breakdowns</summary>", ""))
    for label, metrics, _ in runs:
        lines.extend(breakdown(label, metrics))
    lines.extend(
        (
            "</details>",
            "",
            f"> 📦 Full reports, annotated disassembly and JSON metrics: the `{arguments.artifact_prefix}-*` "
            "workflow artifacts.",
            "",
        )
    )
    return "\n".join(lines)


def main():
    arguments = parse_arguments()
    arguments.output.write_text(compose(arguments), encoding="utf-8")


if __name__ == "__main__":
    main()
