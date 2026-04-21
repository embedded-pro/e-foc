---
# e-foc AI Context Memory — Usage Contract

## Purpose

This directory holds structured knowledge artifacts for AI agents working in the e-foc codebase.
It is **not** a replacement for `documentation/` (which is human-facing architecture and theory)
or `.github/instructions/` (which carries VS Code `applyTo` glob behavior). It is a
machine-queryable supplement that reduces agent mistakes on enforced rules and non-obvious failure
modes, specifically for this embedded FOC project.

Do not add `decisions.jsonl`, `boundaries.jsonl`, or `glossary.jsonl` here. Those concerns are
already well-covered by `documentation/design/`, `documentation/architecture/`, and
`documentation/theory/`.

## Files

| File | What it is |
|---|---|
| `invariants.jsonl` | All enforced rules agents must uphold — one JSON object per line |
| `pitfalls.jsonl` | Non-obvious FOC/embedded failure modes with triggers and mitigations |
| `sources.jsonl` | Blast-radius headers: files whose modification has wide consumer impact |
| `REFERENCE.md` | The original reusable pattern document kept for background reference |

## Agent Bootstrap Protocol (mandatory)

Before starting **any** task in this repository:

1. Read `invariants.jsonl` **in full**. Every entry is short. There are fewer than 25.
2. Scan `pitfalls.jsonl` for entries whose `trigger` field contains keywords matching the task
   (affected file paths, function names, FOC concepts). Read the matching entries in full.
3. If the task modifies any file listed in `sources.jsonl`, read that file's `importedBy` array.
   Treat every listed path as in-scope for the change — do not make a partial update.

## Update Protocol

| What happened | What to update |
|---|---|
| A new enforced rule is established | Add an `invariants.jsonl` entry in the same change set |
| A non-obvious failure mode is discovered in review or post-mortem | Add a `pitfalls.jsonl` entry |
| A new header gains wide adoption across the codebase | Add a `sources.jsonl` entry; populate `importedBy` via grep |
| A blast-radius header gains or loses consumers | Update `importedBy` in the existing `sources.jsonl` entry |

Updates are made by hand — append or edit lines directly. JSONL format: each line is a single
valid JSON object. Validate with:

```
python3 -c "import json; [json.loads(l) for l in open('.github/memory/invariants.jsonl')]"
python3 -c "import json; [json.loads(l) for l in open('.github/memory/pitfalls.jsonl')]"
python3 -c "import json; [json.loads(l) for l in open('.github/memory/sources.jsonl')]"
```

## What This Is Not

- **Not a database.** SQLite, vector indexing, and graph indexing are not needed at the scale of
  this project and are explicitly deferred. See `REFERENCE.md` §3.3 for when they become
  relevant.
- **Not a second copy of the instruction rules.** `invariants.jsonl` is the canonical list of
  enforced rules; `.github/instructions/foc-cpp.instructions.md` provides narrative context and
  links back to the JSONL implicitly.
- **Not a CI gate today.** JSON schema validation and grep-based CI checks (for NiceMock,
  dynamic STL includes, etc.) are noted in individual `enforcement` fields as recommended
  follow-ups but are not yet wired into CI.
