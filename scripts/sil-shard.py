#!/usr/bin/env python3
"""Print the feature files one software-in-the-loop CI shard runs.

The shards are listed in integration_tests/sil-shards.json, balanced by the
time each feature takes. The script refuses a plan that leaves a feature with
an @sil scenario out, lists one twice, names a missing file, or does not have
exactly the number of shards the workflow runs, so a new feature cannot be
skipped silently.

    python3 scripts/sil-shard.py --shard 1 --of 3

Exit codes:
    0  the shard's files are printed, space-separated
    1  the plan is inconsistent (details printed to stderr)
"""

import argparse
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
FEATURES_DIR = ROOT / "integration_tests" / "features"
PLAN = ROOT / "integration_tests" / "sil-shards.json"


def sil_features():
    return {
        path.relative_to(ROOT).as_posix()
        for path in FEATURES_DIR.glob("*.feature")
        if "@sil" in path.read_text(encoding="utf-8").split()
    }


def check(shards, expected_count):
    errors = []
    if len(shards) != expected_count:
        errors.append(f"the plan has {len(shards)} shards, the workflow runs {expected_count}")

    listed = [feature for shard in shards for feature in shard]
    for feature in sorted({f for f in listed if listed.count(f) > 1}):
        errors.append(f"{feature} is listed in more than one shard")
    for feature in sorted(set(listed)):
        if not (ROOT / feature).is_file():
            errors.append(f"{feature} does not exist")
    for feature in sorted(sil_features() - set(listed)):
        errors.append(f"{feature} has @sil scenarios but no shard runs it")

    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--shard", type=int, required=True, help="1-based shard index")
    parser.add_argument("--of", type=int, required=True, help="number of shards the workflow runs")
    args = parser.parse_args()

    shards = json.loads(PLAN.read_text(encoding="utf-8"))["shards"]
    errors = check(shards, args.of)
    if not 1 <= args.shard <= len(shards):
        errors.append(f"shard {args.shard} is outside 1..{len(shards)}")

    if errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        return 1

    print(" ".join(shards[args.shard - 1]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
