# Reusable AI Context Memory Pattern

This plan is intentionally repository-agnostic and intended for reuse across projects.

## 1. Objective

Establish a durable context-memory pattern for AI-assisted engineering that:

- preserves long-lived project knowledge across sessions,
- improves retriever correctness across the full codebase (including shared modules/libraries),
- minimizes repeated context loading costs,
- and keeps memory maintenance inside normal development flow.

## 2. Core Principles

1. Instruction and memory are separate layers.
- Instruction layer: behavior rules, coding standards, and collaboration constraints.
- Memory layer: structured, queryable project knowledge.

2. Source of truth is explicit.
- Canonical source documents remain authoritative.
- Any cache is an optimization layer only.

3. Retrieval is full-scope but budgeted.
- Index all relevant repository domains.
- Retrieve only ranked, bounded context for each task.

4. Memory updates happen with normal changes.
- Avoid separate operational workflows for maintainers.
- Keep memory updates in the same change set as behavior changes.

5. Validation gates enforce drift prevention.
- Missing or stale memory entries should fail automated checks.

## 3. Target Architecture

### 3.1 Layering

- Instruction layer: human-authored policy/configuration documents.
- Context memory layer: structured records for decisions, invariants, boundaries, pitfalls, and glossary terms.
- Retrieval layer: task-aware context pack builder with ranking and token budgeting.
- Freshness layer: source hash tracking and automatic cache invalidation/rebuild.

### 3.2 Co-located Artifact Bundle

Store all memory-related artifacts together in one dedicated directory.

Required artifacts:

- `PATTERN.md` (pattern contract and usage rules)
- `index.json` (schema/version metadata)
- `decisions.jsonl`
- `invariants.jsonl`
- `boundaries.jsonl`
- `pitfalls.jsonl`
- `glossary.jsonl`
- `sources.jsonl` (provenance and source references)
- `standards.cache.json` (optimized retrieval cache)
- `standards.cache.meta.json` (source hashes and generation metadata)
- `CHANGELOG.md` (memory update history)

The artifact files listed above are the schema contract. The canonical storage backend for those
artifacts is SQLite (see §3.3). Field names are designed to map directly to SQLite columns.

### 3.3 Storage Backend

**Canonical default: SQLite**

Use SQLite for all new adoptions. Single-file, embedded, no server, fully local. Provides indexed
queries, schema enforcement, concurrent write-ahead logging. The schemas in §3.4 define the
column structure. This removes the flat-file scale ceiling, the consistency collapse risk, and
the concurrent-write data corruption risk described in §9 before they occur.

**Optional bootstrap mode: Flat-file JSONL**

Use only for initial schema exploration in a single-agent, low-volume repo before committing to a
full SQLite setup. Treat as strictly temporary — not as a first phase that will naturally be
migrated later. Criteria for use:

- Sole adoption purpose is exploring what fields and domains are needed before designing a schema.
- Single agent only. Never appropriate for multi-agent use (see §10.1).
- All domains will stay under ~50 entries total across the whole bundle.
- Expected maintenance period under two weeks.

If any of those criteria cannot be confirmed, start with SQLite. The setup cost of SQLite over
JSONL is measured in minutes. The cost of carrying JSONL past its limits is structural.

Limitations that cannot be mitigated in JSONL: no filtering, no sorting, no concurrent writes,
full-file load on every session. Degradation begins before 200 entries and is not fixable by
prompt engineering or file organization.

**Optional extension: Graph DB** (see §11)

Add when tasks require multi-hop traversal across entities. SQLite remains the authoritative
store; the graph is a derived index. Recommended: Kuzu (embedded, no server, Cypher).

**Optional extension: Vector DB** (see §11)

Add when the knowledge base is large enough that relevant records cannot be located by keyword
alone, or when query vocabulary diverges from stored terminology. The vector index is always
derived; canonical storage remains in SQLite.

### 3.4 Artifact Record Schemas

Each domain has a required field set. Fields use the same names and types as the corresponding
SQLite columns. All fields are strings unless noted. Arrays use JSON array syntax.

**decisions.jsonl** — what was decided and why

```
{"id":"d001","title":"short name","decision":"what was decided","rationale":"why — the most important field; without it the entry is noise","status":"active|superseded|planned","supersededBy":null,"tags":[],"sourceRefs":[],"lastReviewedAt":null,"reviewedBy":null}
```

- `rationale` is mandatory and must be non-empty. A decision without rationale conveys no more
  than reading the code.
- `status: superseded` requires a `supersededBy` ID pointing to the replacement decision.
- `sourceRefs`: paths or document references this decision is derived from. Required for any
  decision that references code, schemas, or external documents.
- `lastReviewedAt` (ISO date) and `reviewedBy`: allow stale detection at entry level, not just
  domain level. An entry not reviewed since a major change to its `sourceRefs` is a candidate
  for re-validation.
- For projects with explicit phases: see §11 for the phase-aware extension fields
  (`activeFromPhase`, `activeUntilPhase`). These are not part of the base schema.

**invariants.jsonl** — what is actually enforced and must never be violated

```
{"id":"i001","statement":"imperative must/must-not statement","scope":"which services or modules","enforcement":"concrete mechanism (CI gate, validation script, code structure)","enforced":true,"violationConsequence":"what breaks if violated","sourceRefs":[],"lastReviewedAt":null,"reviewedBy":null}
```

- `statement` must be imperative (`X must Y`, `X must never Z`), not descriptive (`X does Y`).
  Descriptive entries belong in decisions or boundaries, not invariants.
- `enforcement` must name a concrete mechanism. "Code review" alone is not sufficient.
- `enforced: true` means this constraint is verified by an automated or structural mechanism.
  `enforced: false` means it is upheld only by convention — document it only if the consequence
  of violation is severe enough to warrant the warning, and include a `mitigation` field
  explaining how to enforce it properly.
- **Critical rule:** only document behaviors that are actually enforced. Aspirational behaviors
  that should be invariants but currently have no enforcement mechanism belong in `pitfalls.jsonl`
  with a mitigation of "enforce this." An incorrect invariant is more damaging than a missing
  one: agents will produce changes that violate real constraints while confidently treating the
  result as correct.
- `sourceRefs`, `lastReviewedAt`, `reviewedBy`: same meaning as in decisions. Invariants that
  reference code structures must be re-reviewed when those structures change.

**boundaries.jsonl** — service ownership and write authority

```
{"id":"b001","service":"service or module name","owns":["things exclusively written or controlled"],"readonly":["things read but not owned"],"forbidden":["explicit prohibitions AND factual does-not-do statements"],"migrationStatus":{"from":null,"to":null,"trigger":null,"status":"not-started|in-progress|complete"},"sourceRefs":[],"lastReviewedAt":null,"reviewedBy":null}
```

- `owns` and `forbidden` are the high-value fields. `readonly` provides disambiguation when
  ownership is ambiguous.
- `forbidden` must capture two kinds of entry:
  - **Prohibitions:** rules the service must not violate ("must not write to table X directly").
  - **Negative facts:** things the service factually does not do ("does not write to the
    database at all", "never initiates DB mutations"). These are as important as the
    prohibitions — an agent that does not know what a service cannot do will confidently suggest
    adding capabilities that violate the architecture.
- `migrationStatus` must be a structured object, not a freeform string:
  - `from`: current owner or current state
  - `to`: target owner or target state
  - `trigger`: the condition that gates the cutover (e.g., a phase prerequisite, a done criterion)
  - `status`: `"not-started"` | `"in-progress"` | `"complete"`
  - A null `migrationStatus` means no ownership change is planned for this service/domain.
- `sourceRefs`, `lastReviewedAt`, `reviewedBy`: same meaning as in decisions and invariants.
  Boundaries are one of the highest-risk domains — a stale ownership record will cause an agent
  to write to a service it no longer controls or miss a migration that has already started.

**pitfalls.jsonl** — non-obvious failure modes

```
{"id":"p001","title":"short name","description":"what happens","trigger":"what causes this / when an unsuspecting agent will encounter it","impact":"high|medium|low","mitigation":"what to do instead","sourceRefs":[]}
```

- `trigger` is the critical field. It must describe the specific condition that causes an agent
  to fall into this pitfall. Generic warnings without triggers do not prevent failures.
- One entry per failure mode. Do not group unrelated gotchas into a single entry.
- `sourceRefs` is optional but recommended: a pitfall linked to a specific code path, migration
  record, or incident document is reviewable; one with no source becomes folklore.

**glossary.jsonl** — project-specific terms, IDs, and enums

```
{"term":"ExactTerm","definition":"precise definition","type":"concept|enum|id|process|table|topic|field","disambiguate":null,"sourceRefs":[]}
```

- `disambiguate` names terms that sound similar but mean something different. This is essential
  for abbreviations and domain shorthand that would mislead an outside engineer.
- Only include terms that would confuse an engineer new to this specific project. Do not document
  generic programming concepts.
- `sourceRefs` is optional: link to the file or document where this term is authoritatively
  defined. Useful when a term's meaning is tied to a specific schema, contract, or spec.

**sources.jsonl** — canonical file references per domain

```
{"id":"s001","path":"relative/path/from/repo/root","domain":"memory domain this is authoritative for","purpose":"what this file is and why it matters to an agent","importedBy":[],"importedByNote":null}
```

- Covers shared contracts, schema definitions, validation modules, and key architectural documents.
- `importedBy` communicates blast radius. **Derive this automatically via static import analysis
  wherever tooling supports it** (e.g., `madge`, `dependency-cruiser`, language LSP, or any
  tool that can walk the import graph). Do not maintain a manually curated import list — it will
  drift immediately and silently.
- `importedByNote`: use for non-import-based dependencies that static analysis cannot detect:
  MQTT topic subscriptions, environment variable references, configuration-driven coupling,
  HTTP call targets, documentation cross-references. These cannot be auto-derived and must be
  maintained manually, but they are the minority case, not the default.
- An agent modifying a source file with a populated `importedBy` must treat all listed consumers
  as in-scope before starting.

### 3.5 Agent Bootstrap Protocol

The default bootstrap strategy is **retrieval by task scope**, not full-domain loading. Loading
all domains unconditionally reintroduces the token-cost problem this pattern is designed to solve:
every session burns context budget on facts that are irrelevant to the current task.

**Step 1 — Identify scope**

Before querying memory, identify:
- Which files the task will likely read or modify.
- Which services those files belong to.
- Which memory domains are relevant (e.g., a change to a shared contract file implicates
  `invariants`, `boundaries`, and `sources`; a refactor within one service may only need
  `boundaries` and `pitfalls`).

**Step 2 — Retrieve by scope, in priority order**

Query each domain for records matching the identified scope. Priority order determines what gets
loaded first and what gets dropped under budget pressure:

1. `invariants` — scoped to services affected by the task. Load in full for any system-wide task.
2. `boundaries` — scoped to services the task will write to or call.
3. `decisions` — scoped by tags or service names matching the task area.
4. `pitfalls` — scoped by trigger keywords or service names.
5. `glossary` — scoped to terms that appear in the task description or affected files.
6. `sources` — scoped to files the task will modify or that are referenced by above results.

At SQLite (canonical), this is parameterized queries. At JSONL bootstrap, this is full-file load
(one reason bootstrap mode is limited to small repos where full-file load is acceptable).

**Step 3 — Fall back to full-domain load only when**

- All scoped domains return fewer than ~10 records total (scope is too narrow or memory is sparse).
- The task explicitly affects system-wide concerns (e.g., changing a shared contract, a
  deployment config, or a cross-service invariant).
- A scoped query produces low-confidence results (e.g., unfamiliar area, no tag matches).

**Staleness check**

Before acting on any retrieved record, check whether its `sourceRefs` files have been modified
since `lastReviewedAt`. If yes, re-read the source directly — do not trust a stale entry.

**Context budget rule**

Never drop `invariants` or `boundaries` records that match the task scope. These prevent the
highest-impact mistakes. Drop lower-priority domains first under budget pressure.

### 3.6 Cache Policy

`standards.cache.json` and `standards.cache.meta.json` are generated artifacts, not canonical
memory. Their relationship to canonical memory must be explicitly defined.

**Canonical memory** (SQLite tables or JSONL files if using bootstrap mode) is **repo-tracked**.
It is the source of truth and must be committed to version control.

**Generated cache** (`standards.cache.json`, `standards.cache.meta.json`) may be:
- **Committed** — if cache rebuild is not automated in CI, or if cache generation is expensive.
  Commit the cache alongside canonical memory changes.
- **Gitignored** — if the cache is always rebuilt on demand or in CI before use. Document the
  rebuild command in `PATTERN.md`.

Repository policy determines which. Either is valid; document the choice. Do not leave it
implicit.

**Validation rules:**
- Validation **must fail** on stale canonical memory: entries where `lastReviewedAt` predates
  a known change to `sourceRefs`, missing required entries, or source hash mismatches.
- Validation **must not fail** on an absent generated cache. Absent cache means "rebuild
  needed," not "memory invalid." CI should rebuild the cache after canonical memory updates but
  must not block on a missing cache file as if it were a correctness failure.

## 4. Retriever Correctness Standard

### 4.1 Corpus Scope

Retriever indexing must include the entire project corpus relevant to engineering decisions:

- application/runtime modules,
- supporting services/processes,
- shared packages/libraries,
- contracts/schemas,
- validation/test assets,
- operational and architectural documentation.

### 4.2 Correctness Requirements

1. Domain coverage checks
- Each top-level domain has retrieval anchors and known query matches.
- Shared modules/libraries are first-class retrieval domains, not optional.

2. Cross-boundary query checks
- Queries that depend on contracts and shared definitions return relevant shared artifacts.

3. Freshness checks
- Renamed/moved/deleted sources invalidate stale references.
- Cache is rebuilt when source hashes differ.

4. Budget checks
- Context packs enforce token limits with deterministic truncation.
- Mandatory high-priority items remain included under tight budgets.

### 4.3 Blast-Radius File Identification

Some source files, when changed, break every service that imports them. These are not simply
"shared" — they are system-wide contracts. A small edit to one of these files can silently
invalidate assumptions in every consumer simultaneously, with no local test suite able to catch
the cross-service breakage.

**Identification criteria:** a file is blast-radius if changing it requires coordinated updates
across more than one service boundary, or if its consumers cannot independently validate
correctness after a change.

**Required handling:**

- Every blast-radius file must be listed in `sources.jsonl` with materialized or derivable
  `importedBy` coverage across all consumer services. Prefer auto-derived (see §3.4 sources
  guidance); manually maintained lists are acceptable only for non-import-based coupling.
- Every blast-radius file must have a corresponding `pitfalls.jsonl` entry describing: which
  kinds of change require cross-service coordination, and which consumers must be updated in the
  same change.
- An agent modifying a blast-radius file must load the full `importedBy` list before starting
  and treat every listed consumer as in-scope for that change. Partial updates are not permitted.
- Blast-radius files are high-risk under multi-agent operation (see §10.2). Declare intent and
  check for concurrent modification before proceeding.

## 5. Update Lifecycle (Guardrailed Semi-Automatic)

1. Detect candidate changes
- infer memory-impacting changes from modified files and subsystem boundaries.

2. Propose updates
- generate candidate memory deltas with provenance.

3. Review and approval
- require explicit approval before canonical memory mutation.

4. Apply updates
- write approved changes and record immutable audit entries.

5. Validate
- run schema, freshness, and coverage checks in standard validation pipeline.

For projects using the phase-aware extension (§11): add a phase-transition review step that runs
when a phase boundary is crossed. See §11 for the full procedure.

## 6. Governance Rules

1. Canonical source precedence
- If cache and source disagree, source wins.

2. Same-change maintenance
- Behavior/architecture changes must include corresponding memory updates.

3. No hidden memory writes
- Canonical memory cannot be updated silently without recorded provenance.

4. Data safety boundaries
- Do not store secrets, private credentials, or sensitive personal data in memory artifacts.

5. External source references are advisory unless pinned
- Any `sourceRefs` entry pointing outside the repository (external docs, specs, third-party
  references) must be treated as advisory, not authoritative, unless the reference includes a
  pinned version, commit hash, or archived URL.
- Validation must flag unpinned external refs as advisory. Agents must not treat an unpinned
  external source with the same confidence as a repo-local file — the external document may
  have changed without any signal in the repository.
- If an external source is critical enough to drive an invariant or decision, archive or
  snapshot it and reference the local copy instead.

## 7. Rollout Plan

### Phase A — Core (SQLite)

- Set up SQLite schema with tables for all required domains (§3.4 field definitions).
- Define `index.json` with schema version and domain list.
- Author initial records: invariants, boundaries, decisions, pitfalls, glossary, sources.
- Establish the instruction/memory separation explicitly in `PATTERN.md`.
- Document cache policy (committed or gitignored) in `PATTERN.md`.

### Phase B — Correctness

- Add scoped retrieval queries for each task type in use.
- Add domain coverage tests: each top-level domain returns relevant results for known queries.
- Add source hash freshness checks (stale `lastReviewedAt` vs. source file commits).
- Integrate drift checks into existing validation gates.
- Add blast-radius file identification to `sources.jsonl` (auto-derived via import analysis).

### Phase C — Hardening

- Add auditability and stale-entry reporting (`lastReviewedAt` sweep, missing `sourceRefs`).
- Add quality metrics for retrieval precision and recall.
- Add graph indexing (§11) if relationship traversal queries arise in practice.
- Add vector indexing (§11) if semantic retrieval gaps emerge.
- Tune ranking and context-pack scope from real usage feedback.

## 8. Acceptance Criteria

1. Reusability
- Pattern can be adopted in a new repository without project-specific assumptions.

2. Correctness
- Retrieval tests pass for every top-level domain, including shared modules/libraries.

3. Freshness
- Cache is automatically invalidated/rebuilt on source change.

4. Operational fit
- No extra routine commands required for maintainers beyond existing development/validation flow.

5. Drift resistance
- Validation fails when memory schema, coverage, or freshness constraints are violated.

6. Multi-agent safety
- At Tier 1: documented enforcement that memory files are read-only during task execution and
  that concurrent writes require Tier 2 migration.
- At Tier 2: concurrent read safety verified; write serialization confirmed via WAL mode.
- Work coordination rules documented and understood by all agents operating in the repository.

## 9. Known Limits and Mitigations

1. **Flat-file memory has a hard scale ceiling.**
- Degradation is gradual between ~200 and ~500 entries per domain. The AI begins missing relevant
  context, cost per session increases as context loads grow, and format inconsistencies compound.
  This is a structural limit, not addressable by prompt tuning or file organization.
- Mitigation: treat JSONL as a Phase A starting state only. Design entry schemas from the start
  to map cleanly to SQLite columns. Migrate at the first sign of degradation, not at failure.

2. Runtime-only facts are not fully captured by static repository memory.
- Mitigation: attach runtime probes/log evidence only where needed and avoid mixing with canonical context memory.

3. External knowledge can still be missing.
- Mitigation: track explicit external source references in provenance records.

4. Over-indexing can increase retrieval noise.
- Mitigation: enforce ranking, deduplication, and strict token budgets.

5. **Calling flat-file storage "memory" creates architectural confusion.**
- Markdown files and JSONL files loaded into a context window are instruction/context carriers,
  not a queryable memory system. They lack filtering, sorting, schema enforcement, and concurrent
  access. A system built on this misidentification fails silently at scale with no clear recovery path.
- Mitigation: the instruction/memory separation in §2 and the tier model in §3.3 must be
  understood by everyone who adopts this pattern. Phase A is a starting state; it is not the
  architecture.

## 10. Multi-Agent Coordination

Multi-agent use introduces two separate problems that require separate mitigations: memory storage
concurrency and work coordination. §3.3 addresses the first at the storage tier level. This
section addresses both completely.

### 10.1 Memory Storage Concurrency

At **Tier 1 (JSONL)**, two agents writing to the same file simultaneously cause silent data
corruption. There is no error, no lock contention signal, and no recovery path. The file is
silently malformed or truncated.

- **Rule:** at Tier 1, memory files are read-only during any task execution. Only one agent may
  propose memory writes at a time, and writes are applied at task end through the guardrailed
  lifecycle in §5 — never mid-task, never by two agents simultaneously.
- **Hard migration trigger:** the moment more than one agent is concurrently active against the
  same repository, migrate to SQLite (Tier 2) regardless of entry count. This is a data integrity
  requirement, not a performance optimization.

At **Tier 2 (SQLite WAL)**, concurrent reads are safe and writes are serialized automatically.
The storage corruption risk is eliminated. The work coordination problem below still applies at
all tiers.

### 10.2 Work Coordination

Storage safety does not solve the problem of agents producing conflicting or incoherent changes.

**Context coherence**

Each agent must re-read memory at task start. Do not carry context from a prior session.
Another agent's recent commits may have changed the system state that memory describes, and stale
context produces changes that conflict with the current codebase state rather than advancing it.

Before starting a task, identify which source files listed in `sources.jsonl` are relevant to the
task's affected area. Check whether any of those files have been modified since the memory's
`generatedAt` timestamp. If yes, re-read those files directly before acting — do not trust the
memory entry for that domain.

**Work area isolation**

Agents working simultaneously should operate on separate branches or git worktrees to prevent
mid-task interference. If agents must share a branch, each agent must declare which files it
intends to modify before starting. Overlapping declared work areas require human coordination
before either agent proceeds.

**Memory update conflicts**

If two agents independently propose changes to the same memory entry, neither proposal should
auto-apply. Both proposals require human review before any write is committed. Treat concurrent
memory update proposals the same as merge conflicts: surface them for review, do not resolve
them automatically by taking the later write.

Agents must not amend or overwrite another agent's memory changes without explicit review.

**Shared contract files**

Files that define cross-service contracts — topics, actions, event schemas, delivery profiles,
validation rules — are disproportionately high-risk under multi-agent operation. A change to any
of these affects every subscriber and consumer in the system.

Any agent modifying a shared contract file must:
1. Declare the intent before starting.
2. Confirm no other agent is concurrently modifying the same contract or any file that depends
   on it.
3. Update all consumer-side expectations in the same change. Do not defer consumer updates to a
   follow-up task.

**Validation gates**

Each agent must run the full validation suite before proposing memory updates or marking a task
complete. One agent's in-progress changes can cause another agent's passing tests to fail on
the combined state. If the repository includes a deployment contract check, it must pass on the
merged state of all concurrent agents' changes — not on each agent's isolated branch alone.

## 11. Optional Extensions

These extensions are not part of the base pattern. Adopt them only when a concrete need arises.
Adding them speculatively increases maintenance burden without improving agent output.

### 11.1 Phase-Aware Extension

For projects organized around explicit, named implementation phases where decisions have bounded
validity (e.g., "correct in Phase 3, superseded in Phase 4").

**Additional fields for `decisions.jsonl`:**

```
{"activeFromPhase":null,"activeUntilPhase":null}
```

- `activeUntilPhase`: this decision is correct now but will be superseded when the named phase
  begins. An agent must check this before treating a current-state decision as the long-term
  target. Acting on a future-phase decision in the current phase is as wrong as ignoring it when
  that phase arrives.
- `activeFromPhase`: this decision is planned but not yet in effect.

**Phase-transition review** (add as step 6 of the §5 update lifecycle when this extension is
active):

When a phase begins or completes, before any agent starts work on the new phase:
- All decisions where `activeUntilPhase` matches the new phase: set `status: superseded`,
  populate `supersededBy`.
- All decisions where `activeFromPhase` matches the new phase: set `status: active`.
- All boundaries where `migrationStatus.trigger` references the completed phase: update
  `migrationStatus.status` and revise `owns`/`forbidden` to reflect new ownership.
- Any invariants whose scope or enforcement changes under the new phase.

Phase-transition reviews must complete before any code changes targeting the new phase begin.

### 11.2 Graph Index Extension

For projects where tasks require multi-hop traversal: which decisions constrain which boundaries,
which boundaries depend on which contracts, which services are affected by a given change.

- Recommended backend: Kuzu (embedded, no server, Cypher query language).
- SQLite remains the authoritative store. The graph is a derived index rebuilt from SQLite.
- Adopt when scoped SQLite queries repeatedly fail to surface relevant context because the
  connection between records spans multiple hops.

### 11.3 Vector Index Extension

For projects where the knowledge base is large enough that relevant records cannot be located
by keyword alone, or when query vocabulary diverges systematically from stored terminology.

- The vector index is always a derived layer. Canonical storage remains in SQLite.
- Adopt when scoped keyword queries produce low recall on known-relevant records, not
  speculatively.

