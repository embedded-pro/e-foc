#!/usr/bin/env python3
"""
generate-traceability-matrix.py

Generates a requirements-vs-SIL traceability matrix in Markdown.

Usage:
    python3 generate-traceability-matrix.py \
        --requirements-dir documentation/requirements \
        --features-dir    integration_tests/software_in_the_loop/features \
        [--junit-xml      sil-test-report.xml] \
        --output          traceability-matrix.md

The requirements are loaded from all *.yaml files under --requirements-dir.
Each YAML file must be a YAML list whose items have at minimum:
    - id: REQ-FOC-002
      title: Independent Id and Iq current control
      shall: >
        ...

Feature files under --features-dir are scanned for lines matching
    # REQ-XXX-NNN[, REQ-YYY-NNN ...]
appearing immediately before a "Scenario:" line.  Each such tag maps the
scenario to the requirement.

If --junit-xml is supplied the script parses it to determine whether each
mapped scenario passed or failed, and adds a status column to the table.
"""

from __future__ import annotations

import argparse
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

import yaml  # PyYAML — available in the CI environment via pip


# ── Data loading ──────────────────────────────────────────────────────────────

def load_requirements(requirements_dir: Path) -> dict[str, dict]:
    """Return {req_id: {id, title, shall, domain}} for all YAML files."""
    reqs: dict[str, dict] = {}
    for yaml_file in sorted(requirements_dir.rglob("*.yaml")):
        domain = yaml_file.stem
        with yaml_file.open(encoding="utf-8") as fh:
            items = yaml.safe_load(fh) or []
        if not isinstance(items, list):
            continue
        for item in items:
            req_id = item.get("id", "").strip()
            if req_id:
                reqs[req_id] = {
                    "id": req_id,
                    "title": item.get("title", ""),
                    "shall": item.get("shall", "").strip().replace("\n", " "),
                    "domain": domain,
                }
    return reqs


_REQ_TAG_RE = re.compile(r"REQ-[A-Z]+-\d+")
_SCENARIO_RE = re.compile(r"^\s*Scenario(?:\s+Outline)?:\s*(.+)", re.IGNORECASE)


def load_feature_mapping(features_dir: Path) -> dict[str, list[str]]:
    """Return {req_id: [scenario_name, ...]} by scanning feature files."""
    mapping: dict[str, list[str]] = {}
    for feature_file in sorted(features_dir.glob("*.feature")):
        lines = feature_file.read_text(encoding="utf-8").splitlines()
        pending_reqs: list[str] = []
        for line in lines:
            tag_matches = _REQ_TAG_RE.findall(line)
            if tag_matches:
                # Comments may carry multiple IDs; accumulate until Scenario:
                pending_reqs.extend(tag_matches)
                continue
            scenario_match = _SCENARIO_RE.match(line)
            if scenario_match and pending_reqs:
                scenario_name = scenario_match.group(1).strip()
                for req_id in pending_reqs:
                    mapping.setdefault(req_id, []).append(scenario_name)
                pending_reqs = []
            elif not line.strip().startswith("#") and line.strip():
                # Non-comment, non-empty, non-scenario line clears pending tags
                pending_reqs = []
    return mapping


def load_junit_results(junit_xml: Path) -> dict[str, str]:
    """Return {scenario_name: 'pass'|'fail'|'skip'} from a JUnit XML file."""
    if not junit_xml.exists():
        return {}
    tree = ET.parse(junit_xml)
    root = tree.getroot()
    results: dict[str, str] = {}
    # Support both <testsuite> and <testsuites> as root element
    test_cases = root.iter("testcase")
    for tc in test_cases:
        name = tc.get("name", "").strip()
        if not name:
            continue
        if tc.find("failure") is not None or tc.find("error") is not None:
            status = "fail"
        elif tc.find("skipped") is not None:
            status = "skip"
        else:
            status = "pass"
        results[name] = status
    return results


# ── Matrix generation ─────────────────────────────────────────────────────────

_STATUS_ICON = {
    "pass": "PASS",
    "fail": "FAIL",
    "skip": "SKIP",
}


def _scenario_status(scenarios: list[str], junit: dict[str, str]) -> str:
    """Aggregate all scenario statuses for a requirement."""
    if not junit:
        return "—"
    statuses = {junit.get(s, "unknown") for s in scenarios}
    if "fail" in statuses:
        return f"{_STATUS_ICON['fail']}"
    if "unknown" in statuses:
        return "PARTIAL"
    if "skip" in statuses and "pass" not in statuses:
        return f"{_STATUS_ICON['skip']}"
    return f"{_STATUS_ICON['pass']}"


def generate_matrix(
    reqs: dict[str, dict],
    feature_map: dict[str, list[str]],
    junit: dict[str, str],
) -> str:
    """Produce the full Markdown traceability matrix document."""
    include_status = bool(junit)

    lines: list[str] = [
        "# Requirements Traceability Matrix",
        "",
        "This document maps every requirement to the SIL scenarios that verify it.",
        "",
    ]

    # Group by domain
    domains: dict[str, list[dict]] = {}
    for req in reqs.values():
        domains.setdefault(req["domain"], []).append(req)

    uncovered: list[str] = []

    for domain, domain_reqs in sorted(domains.items()):
        domain_title = domain.replace("-", " ").title()
        lines += [f"## {domain_title}", ""]

        header = "| ID | Title | Scenarios |"
        sep    = "|---|---|---|"
        if include_status:
            header = "| ID | Title | Scenarios | Status |"
            sep    = "|---|---|---|---|"

        lines += [header, sep]

        for req in sorted(domain_reqs, key=lambda r: r["id"]):
            req_id = req["id"]
            title = req["title"]
            scenarios = feature_map.get(req_id, [])
            if not scenarios:
                uncovered.append(req_id)
                scenario_text = "_not covered_"
            else:
                scenario_text = "<br>".join(f"`{s}`" for s in scenarios)

            if include_status:
                status = _scenario_status(scenarios, junit)
                lines.append(f"| `{req_id}` | {title} | {scenario_text} | {status} |")
            else:
                lines.append(f"| `{req_id}` | {title} | {scenario_text} |")

        lines.append("")

    # Uncovered summary
    if uncovered:
        lines += [
            "## Uncovered Requirements",
            "",
            "The following requirements have no scenario tagged in the feature files:",
            "",
        ]
        for req_id in sorted(uncovered):
            lines.append(f"- `{req_id}` — {reqs[req_id]['title']}")
        lines.append("")

    # Coverage statistics
    total = len(reqs)
    covered = total - len(uncovered)
    pct = int(100 * covered / total) if total else 0
    lines += [
        "## Coverage Summary",
        "",
        f"- **Total requirements**: {total}",
        f"- **Covered by scenarios**: {covered} ({pct}%)",
        f"- **Not covered**: {len(uncovered)}",
        "",
    ]

    return "\n".join(lines)


# ── Entry point ───────────────────────────────────────────────────────────────

def main() -> int:
    parser = argparse.ArgumentParser(description="Generate requirements traceability matrix")
    parser.add_argument(
        "--requirements-dir",
        default="documentation/requirements",
        help="Directory containing *.yaml requirement files (default: documentation/requirements)",
    )
    parser.add_argument(
        "--features-dir",
        default="integration_tests/software_in_the_loop/features",
        help="Directory containing *.feature scenario files",
    )
    parser.add_argument(
        "--junit-xml",
        default=None,
        help="Path to JUnit XML from SIL test run (optional)",
    )
    parser.add_argument(
        "--output",
        default="traceability-matrix.md",
        help="Output Markdown file path (default: traceability-matrix.md)",
    )
    args = parser.parse_args()

    req_dir = Path(args.requirements_dir)
    feat_dir = Path(args.features_dir)

    if not req_dir.is_dir():
        print(f"ERROR: requirements directory not found: {req_dir}", file=sys.stderr)
        return 1
    if not feat_dir.is_dir():
        print(f"ERROR: features directory not found: {feat_dir}", file=sys.stderr)
        return 1

    reqs = load_requirements(req_dir)
    if not reqs:
        print("ERROR: no requirements loaded — check YAML files", file=sys.stderr)
        return 1

    feature_map = load_feature_mapping(feat_dir)

    junit: dict[str, str] = {}
    if args.junit_xml:
        junit_path = Path(args.junit_xml)
        if not junit_path.exists():
            print(f"WARNING: JUnit XML not found: {junit_path} — status column omitted", file=sys.stderr)
        else:
            junit = load_junit_results(junit_path)

    matrix_md = generate_matrix(reqs, feature_map, junit)

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(matrix_md, encoding="utf-8")
    print(f"Traceability matrix written to: {output_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
