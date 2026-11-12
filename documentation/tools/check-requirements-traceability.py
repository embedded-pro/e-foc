#!/usr/bin/env python3
"""Trace requirements to the feature scenarios that verify them.

Lists every requirement ID with the @REQ tags found in integration_tests/features
and fails on a duplicate ID, a requirement without a status or a verification, a
`shall` longer than MAX_SHALL_WORDS words or naming another requirement, a tag naming
an unknown or retired ID, a requirement with `status: verified` and no verifying tag,
and a scenario kind (@sil, @hil) the requirement's `verification` field does not
declare. Standard library only.
"""

import argparse
import collections
import pathlib
import re
import sys

STATUSES = {"draft", "implemented", "verified", "retired"}
KINDS = {"sil", "hil", "mil", "unit", "inspection"}
TAG_KINDS = {"sil": "sil", "hil": "hil", "hil_motor": "hil"}
REQ_TAG = re.compile(r"@(REQ-[A-Z]+-[0-9]{3})\b")
REQ_REFERENCE = re.compile(r"\bREQ-[A-Z]+-[0-9]{3}\b")
MAX_SHALL_WORDS = 90
SCENARIO_KEYWORDS = ("Scenario:", "Scenario Outline:", "Scenario Template:", "Example:")


def parse_requirements(path):
    entries = []
    retired = []
    lines = path.read_text(encoding="utf-8").splitlines()
    index = 0
    while index < len(lines):
        line = lines[index]
        index += 1
        retired_match = re.match(r"#\s+(REQ-[A-Z]+-[0-9]{3}):", line)
        if retired_match:
            retired.append(retired_match.group(1))
            continue
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        item = re.match(r"- (\w+):\s*(.*)$", line)
        field = re.match(r"  (\w+):\s*(.*)$", line)
        match = item or field
        if not match:
            raise ValueError(f"{path}:{index}: unsupported line {line!r}")
        key, value = match.groups()
        if item:
            entries.append({"_file": str(path), "_line": index})
        if value in (">", "|", ">-", "|-"):
            block = []
            while index < len(lines) and (not lines[index].strip() or lines[index].startswith("    ")):
                block.append(lines[index].strip())
                index += 1
            value = " ".join(part for part in block if part)
        elif value.startswith("["):
            value = [part.strip() for part in value.strip("[]").split(",") if part.strip()]
        else:
            value = value.strip("\"'")
        entries[-1][key] = value
    return entries, retired


def parse_features(root):
    scenarios = []
    for path in sorted(root.glob("*.feature")):
        feature_tags = set()
        pending = set()
        current = None
        seen_feature = False
        for number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            line = raw.strip()
            if line.startswith("@"):
                pending.update(line.split())
            elif line.startswith("Feature:"):
                feature_tags, pending, seen_feature = set(pending), set(), True
            elif line.startswith(SCENARIO_KEYWORDS):
                current = {"file": path.name, "line": number, "tags": feature_tags | pending}
                scenarios.append(current)
                pending = set()
            elif line.startswith(("Examples:", "Scenarios:")):
                if current is not None:
                    current["tags"] |= pending
                pending = set()
            elif line and not line.startswith("#"):
                pending = set()
        if not seen_feature:
            raise ValueError(f"{path}: no Feature line")
    return scenarios


def check(entries, retired, scenarios):
    errors = []
    by_id = {}
    for entry in entries:
        requirement_id = entry.get("id", "")
        where = f"{entry['_file']}:{entry['_line']}"
        if requirement_id in by_id:
            errors.append(f"{where}: duplicate ID {requirement_id}")
        by_id[requirement_id] = entry
        status = entry.get("status")
        if status is None:
            errors.append(f"{where}: {requirement_id} has no status")
        elif status not in STATUSES:
            errors.append(f"{where}: {requirement_id} has unknown status {status!r}")
        declared = entry.get("verification", [])
        declared = [declared] if isinstance(declared, str) else declared
        if not declared:
            errors.append(f"{where}: {requirement_id} has no verification")
        elif any(kind not in KINDS for kind in declared):
            errors.append(f"{where}: {requirement_id} has unknown verification {declared!r}")
        entry["_declared"] = set(declared)
        shall = entry.get("shall", "")
        if len(shall.split()) > MAX_SHALL_WORDS:
            errors.append(f"{where}: {requirement_id} has a shall of {len(shall.split())} words, over {MAX_SHALL_WORDS}")
        if REQ_REFERENCE.search(shall):
            errors.append(f"{where}: {requirement_id} names another requirement in its shall")
    for requirement_id in retired:
        if requirement_id in by_id:
            errors.append(f"{requirement_id} is listed as retired and still defined")

    tags = collections.defaultdict(list)
    for scenario in scenarios:
        kinds = {TAG_KINDS[tag[1:]] for tag in scenario["tags"] if tag[1:] in TAG_KINDS}
        for tag in sorted(scenario["tags"]):
            match = REQ_TAG.fullmatch(tag)
            if match:
                tags[match.group(1)].append((scenario, kinds))

    for requirement_id, uses in sorted(tags.items()):
        entry = by_id.get(requirement_id)
        for scenario, kinds in uses:
            where = f"{scenario['file']}:{scenario['line']}"
            if entry is None:
                reason = "retired" if requirement_id in retired else "unknown"
                errors.append(f"{where}: tag @{requirement_id} names a {reason} requirement")
            elif entry.get("status") == "retired":
                errors.append(f"{where}: tag @{requirement_id} names a retired requirement")
            elif entry.get("verification") is not None and not kinds <= entry["_declared"]:
                missing = sorted(kinds - entry["_declared"])
                errors.append(f"{where}: @{requirement_id} is verified by {missing} which its verification field omits")
    for requirement_id, entry in by_id.items():
        if entry.get("status") == "verified" and requirement_id not in tags:
            errors.append(f"{entry['_file']}:{entry['_line']}: {requirement_id} is verified and no scenario carries @{requirement_id}")
    return errors, by_id, tags


def print_listing(by_id, tags):
    print("Requirement traceability")
    for requirement_id in sorted(by_id):
        entry = by_id[requirement_id]
        files = sorted({f"{scenario['file']}:{scenario['line']}" for scenario, _ in tags.get(requirement_id, [])})
        status = entry.get("status", "-")
        print(f"  {requirement_id:<13} {status:<12} {len(files):>3}  {', '.join(files) if files else '-'}")


def print_summary(by_id, tags):
    prefixes = collections.defaultdict(list)
    for requirement_id in by_id:
        prefixes[requirement_id.split("-")[1]].append(requirement_id)
    columns = ("total", "verified", "implemented", "draft", "retired", "unmarked", "tagged")
    print()
    print("Coverage by prefix")
    print(f"  {'prefix':<8}" + "".join(f"{name:>13}" for name in columns))
    totals = collections.Counter()
    for prefix in sorted(prefixes):
        ids = prefixes[prefix]
        counts = collections.Counter(by_id[i].get("status", "unmarked") for i in ids)
        counts["total"] = len(ids)
        counts["tagged"] = sum(1 for i in ids if i in tags)
        totals.update(counts)
        print(f"  {prefix:<8}" + "".join(f"{counts[name]:>13}" for name in columns))
    print(f"  {'all':<8}" + "".join(f"{totals[name]:>13}" for name in columns))


def main():
    root = pathlib.Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--requirements-dir", type=pathlib.Path, default=root / "documentation/requirements")
    parser.add_argument("--features-dir", type=pathlib.Path, default=root / "integration_tests/features")
    parser.add_argument("--quiet", action="store_true", help="print only the summary and the errors")
    args = parser.parse_args()

    entries, retired = [], []
    for path in sorted(args.requirements_dir.rglob("*.yaml")):
        file_entries, file_retired = parse_requirements(path)
        entries += file_entries
        retired += file_retired
    errors, by_id, tags = check(entries, retired, parse_features(args.features_dir))

    if not args.quiet:
        print_listing(by_id, tags)
    print_summary(by_id, tags)
    for error in errors:
        print(f"ERROR: {error}", file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
