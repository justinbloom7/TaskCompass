<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Phase 1.3 — Everyday task management

**Current status — Stage A accepted. Stage B Linux and Windows offline validation passed. Owner acceptance is pending.**
The owner approved the Stage A recommended defaults and authorized only Stage B
source and database-free validation. See the [Stage B record](#stage-b-contexts-and-shared-persistence).
See the [native Windows result](#2026-11-23-native-windows-stage-b-result).
Stage C and every database execution require separate explicit authorization.

## Baseline and authorization

The inspected branch is `jb_20261122_ms1p3`, tracking the same remote branch.
The initial worktree was clean at `7849827ae7f42fa0979364a3619b793f3a346782`.
[Phase 1.1](Phase1.1.md) was accepted on 2026-11-14.
[Phase 1.2](Phase1.2.md) was accepted on Linux and Windows on 2026-11-22
following B7.1–B7.9 and [B7.10 reconciliation](../Phase1.2B710.md).
E1 accepts the protected task's `updatedAt`, `localRevision` and `objectiveId`
variance. The original checkpoint remains **FAILED and unsealed**. E2 accepts
functional Linux restart persistence. The historical before/after byte
comparison remains **NOT CAPTURED**. Preserve both exceptions, existing
databases and accounts, restored databases, backups and all historical artifacts.
The recorded platforms are CentOS Stream 9 with MariaDB 10.5.29 and native
Windows MSYS2 UCRT64 with MariaDB 13.0.2. These are historical owner results.

### Requirements and discrepancies

- [Requirements](../Requirements.md), [Interactions](../Interactions.md),
  [Data model](../DataModel.md) and [Roadmap](../Roadmap.md) establish optional
  primary task context, user-defined context order with No context last,
  positive numeric task priorities defaulting to 5, hidden persistent manual
  ordering, and atomic previewed single-task moves. Contexts do not own tasks.
- Context archive preserves assignments and order. Default grouping treats
  archived assignments as unassigned. Delete requires impact confirmation and
  tombstones context and live assignments together. Safe undo restores the same
  IDs. No seeded contexts, synthetic database context, or arbitrary priority cap.
- Worked Today reflects work history. Done for Today and Resume Today are
  independent daily intent. Completion changes lifecycle and records completion
  history without asserting progress. Daily intent requires no scheduled date.
- Preserve existing organizational ownership, views, task workflows, and the
  in-memory prototype. Defer due and scheduled dates, allocations, calendar
  week-start and planning horizons, recurrence, secondary contexts, per-day
  manual overrides, sync/mobile and durable reconstruction of the undo stack.
- README still describes Phase 1.2 source review and pending platform validation.
  The final milestone and B7.10 acceptance govern. README's “derived context”
  refers to organizational ancestry. A primary user-defined Context is a separate
  relation. Propose clarifying both points in a later authorized README update.
- Interaction examples that close/reopen allocations describe behavior once
  allocations exist. Schema 2 has none, and the data model explicitly defers
  integration to 1.4. Phase 1.1 completion history lives in CommandRecord.
  Propose retaining that evidence and writing completion WorkEntry rows only for
  commands issued in schema 3. Do not invent dates or fabricate legacy sessions.
- Current persistent task table sorts by column 0 (title) on startup and provides
  search, owner, archive and completion filtering. Grouped manual ordering is a
  new approved behavior. Keep title sorting available. No conflict found between
  the requested drag rules and their approved repository counterparts. The
  treatment of hidden rows and collapsed headers needs the explicit policy below.

## Existing architecture and proposed extension

[Architecture](../Architecture.md), [build configuration](../../CMakeLists.txt)
and source separate standard C++20 domain/application from Qt Widgets and Qt SQL.
The persistent executable uses
[CommandService](../../src/application/CommandService.cpp),
[WorkspaceRepository](../../src/application/WorkspaceRepository.h),
[MariaDbWorkspaceRepository](../../src/infrastructure/storage/MariaDbWorkspaceRepository.cpp)
and [WorkspaceCodec](../../src/infrastructure/storage/WorkspaceCodec.cpp).
The legacy TaskService/TaskRepository path supports earlier tests and schema 1.
The production path uses a coherent workspace read, a database-scoped advisory
mutation lease, pure command preparation, one target CAS write and immutable
CommandRecord/LocalChange writes in a transaction. The chronological undo stack is
session-local. Inverse commands validate revisions and write compensating history.
The persistent controller reloads committed data and handles uncertain commits.
The table model and filter proxy currently expose flat task rows alongside a
separate organizational tree.

Extend the existing workspace pipeline and session rather than creating a second
command path. Extend workspace snapshots with context, assignment and current-day
projections. Load work history on demand by task/date with indexed access. Any
split read used to present a consistent view must share a coherent transaction.
Keep rank arithmetic, effective grouping, command and dependency validation in
standard C++. Widgets provide accessible intent and presentation. Qt SQL owns
mapping, locking and atomic persistence. The existing snapshot-wide revisions
are useful for Phase 1.2 commands. Use narrowly scoped topology preconditions for
Phase 1.3 and keep existing safeguards for organizational operations.

## Proposed canonical ranks and context behavior

Use variable-length exact dyadic fractions in (0,1). Encode binary fractional
digits as ASCII `0` and `1`, omitting the decimal point and trailing zero bits.
Nonempty, ends in `1`, at most 128 bytes in stored ranks. Leading zero bits are
significant. Reject malformed external input and persisted corruption. Only
arithmetic results are canonicalized by trimming trailing zero bits. Compare
unsigned bytes lexicographically with a shorter prefix first. A canonical prefix
represents the same prefix padded with zeros and is strictly smaller than any
canonical extension. Therefore numeric order equals byte order. MariaDB stores
VARBINARY and binds bytes through QMYSQL. No floating point or locale collation.

- Represent missing ends by unencodable 0 and 1. Generate the exact midpoint by
  aligning both finite binary fractions, adding with carry and dividing by two.
  Use bounded bit arithmetic with room for carry and a 129th fractional bit.
  Between `01` and `1` produces `011`. Before `01` produces `001`.
  After `1` produces `11`. An empty band gets `1`. Reject reversed bounds.
- Equal-ranked rows use canonical lowercase UUID byte order. A tied anchor pair,
  a midpoint exceeding 128 bytes or a collision with an occupied destination
  token invokes normalization. No token uniqueness constraint on Context or Task.
- Start with immediate affected neighbors in the destination band. Grow a
  contiguous window symmetrically, extending inward when one end is missing, to
  at most 64 existing participants plus the incoming row. Include both neighbors
  of the intended gap when present and keep exterior rows fixed. Find a grid
  `j/2^p` for the smallest precision `p <= 128` with enough interior points.
  For first admissible grid integer L and M available integers, assign slot k
  the integer L + floor(k * (M + 1) / (n + 1)) - 1, with n rows and k from 1
  through n. This is strictly increasing when M >= n. Assign in prior tuple
  order with the moving row inserted at its intended position. Verify strict bounds and adjacency, including exterior
  ranks and IDs, before committing. A degenerate exterior interval or inability
  to find enough grid points grows the window within the bound. If no window
  succeeds, reject without changes and report the affected band. Any wider
  repair would require a separate reviewed procedure.
- Avoid a large initial collision set by seeding legacy ranks in stable UUID
  order per dataset. For N old tasks, choose the smallest precision with
  `2^p > N`. Assign row i the binary fraction `i / 2^p`, i starting at 1,
  and canonicalize. Include all old lifecycle/tombstone rows. Migration uses
  the same encoder and exact integer capacity checks. All legacy priorities are 5.
- Apply identical arithmetic to Context order. Live archived contexts retain
  their ranks and reserved positions. Delete retains historical rank. Restore
  reuses it and resolves any equal rank by UUID. Context creation appends.
  Blank or invalid context names are rejected; proposed duplicate names allowed
  with destination disambiguation by presentation. Direct assignment appends to
  the destination priority band, direct priority edit appends to the new band,
  and new tasks append within their default band.
- Sort by assigned first, Context rank then Context UUID, ascending positive
  priority, task rank and task UUID. Use signed 64-bit priority with a technical
  range of 1–9223372036854775807, exact decimal validation and serialization.
  Use a validated text editor for that range and present input errors.
  Default archive/unarchive/delete recomputes effective group membership without
  rewriting task ranks. Merged equal keys retain deterministic UUID order.
  **Owner decision:** every drop into No context clears the live primary link,
  including an archived assignment and a move within the effective No context
  group. Preview the link being removed, naming the archived context when
  applicable. A same-position drop with such a link is a real command.

## Grouped movement and accessibility

Add group and empty-target presentation to the persistent task pane, retaining
the organizational sidebar and title sort. Prefer a grouped Qt model with
context/child identities and a flat presentation mode for alternate sort, with
the final model choice and native accessibility checked in Stage A. Model roles
and MIME carry validated stable IDs, source revisions and workspace identity.
Resolve proxies to identities. Calculate previews without writes. A line between
rows and status text identify destination, resulting priority and exact neighbor.
Support autoscroll, head/tail/empty groups, and a distinct No context target.
A collapsed header means end of group and previews that meaning. Use accurate
Qt move/layout notifications and preserve selection by ID after refresh.

On drop, remove the moving row conceptually before resolving the visual anchors.
Use successor priority before a successor, predecessor priority at end, and
retain priority for an empty destination. Then resolve the closest actual
neighbors in the destination priority band. If the anchor is visible but rows
are hidden by default lifecycle or organizational archive rules, insert
immediately before the successor or after the predecessor in the full effective
band. An empty visible group with hidden occupants appends in the retained
priority band. The full band includes all non-tombstoned tasks of that effective
group for the current grouping policy. Preview the visible anchor and validate
the actual gap at commit. Default archive policy is the only reorder projection
proposed for 1.3. Disable reorder in alternate sort, search, owner scope,
explicit context/priority filters, archive expansion and completed-history views.
Offer return to manual view and keyboard Move Before/After or Move to Context
with the same preview, eligibility and command. Invalid/stale/canceled drops
and hover perform zero writes. A valid no-op has no history entry.

## Daily work and disposition proposal

Use an injectable UTC clock and calendar resolver. Propose a persisted per-user
desktop setting with a validated IANA zone initialized from the device zone.
Stage A must verify identifier mapping, zone data and DST support on native
Windows and Linux. Surface unavailable zones for correction and use an explicit
zone selection before accepting a date-sensitive command if no valid zone can
be resolved. At command intent start capture one UTC instant, zone ID and
Gregorian local `YYYY-MM-DD` derived from that pair. Store date in SQL DATE and
UTC instants as epoch milliseconds. Read the same capture through preparation and
commit. If a date-sensitive dialog crosses configured-zone midnight or the
configured zone changes, require the operator to confirm the captured date or
start with a fresh capture. Immediate commands keep the issue date even when
commit crosses midnight. Refresh labels at zone midnight, zone changes and
resume, capturing the current day again for new intents. History keeps its
original dates and zones. Undo/redo uses receipt target/date/zone and records a
fresh UTC audit timestamp. Day identity is task plus date, regardless of zone.

| Intent | Proposed transaction and state |
| --- | --- |
| Worked Today on | Insert one live quick marker per task/date when absent. Duplicate quick activation is a no-op. Reject new work on a closed day and offer Resume. Derived indicator is true if any live progress, session or closure marker exists. |
| Worked Today off | Void only the live quick marker and add a correction record. Show remaining work in history if it keeps the indicator on. When there is no quick marker, offer explicit history correction. |
| Record a session | Use a simple Add work entry action in task history, with an optional note. Multiple entries share one date without sharing identity. Timers, editable session intervals, duration and reporting are outside Stage A scope. |
| Correct a session | Void original and add append-only correction plus optional replacement in one transaction. Preserve original payload and its captured date/zone. Inverse operations revise the void projection and append audit, with stale-state guards. Completion entries are owned by lifecycle commands. |
| Done for Today | Require active task. Set day closed with closure command ID. Insert a closure progress marker only if no valid work exists. Duplicate closure is a no-op. Keep task active. |
| Resume Today | Require active task and a closed disposition for captured date. Set open, clear current closure ID, preserve work and closure history. An absent day is implicitly open. |
| Complete | Set lifecycle and completedAt and insert a completion event. Preserve any daily closure or work. The completion event alone cannot check Worked Today. Reject duplicate lifecycle transition under existing semantics. |
| Reopen | Restore active lifecycle, clear current completedAt, preserve completion event and any daily closure. A reopened task still closed on that date requires explicit Resume. Undo of completion restores the lifecycle tuple and compensates its specific completion event. |

Propose one default canonical list with closed badges and a separate
closed-today view. During A settle this choice or adopt closed subsections
within each context with reorder disabled in that projection. Also
settle whether history corrections may leave a closed day with no live work
(recommend yes, since closure and progress are distinct), marker/session controls,
and the display of legacy completion history.

## Multi-record receipts, undo and concurrency

Extend WorkspaceReceipt with one root (Task or Context for this scope) and an
ordered set of typed before/after patches, field masks, expected revisions or
expected absence, and intent metadata. Persist the same patch set and operation
identity in a version-3 audit/outbox envelope. Keep version 1/2 readers and their
original bytes. Encode int64 values as exact decimal strings in JSON, binary
ranks as ASCII strings, local dates as date strings and nullable values explicitly.
Keep a root revision for every committed intent, including work/day commands.
Advance only the root and actual patch participants. Work/day changes may
advance the task revision/timestamp without changing its ownership, lifecycle,
priority or rank. Review that timestamp policy with the owner.

Record source and destination group identity and visibility, prior/new
assignment (including archived link), priority and rank, visible anchors and
actual gaps, anchor revisions, normalization participants and their before/after
ranks, and all date or correction provenance. Dependency guards include
affected Context/TaskContext state and original/target placement boundaries,
plus relevant absence and membership predicates. Empty group and unassigned
targets have no row lock representing the gap. Recompute predicates from
fresh committed state under the existing mutation lease. Check context deletion
impact again under the lease against the approved link set. Generate preview
from a coherent read, revalidate source, destination and adjacency on drop,
then CAS-write touched rows, journal and outbox together. Use bound parameters
and stable write order, inserting the command root before rows referencing its
audit identity. The lease serializes supported writers including competing empty
gap insertions. Restrict runtime grants and treat out-of-band SQL as outside the
supported write protocol.

Undo/redo is one session stack across task, organization, context, move, work
and day changes. Compare current masked state and revisions with the expected
side of the receipt and validate affected membership, actual gaps, marker
uniqueness and archive/lifecycle rules. Track revisions from successful
same-session compensation and update dependencies only when logical state and
topology still match. External changes to touched rows, relevant anchors or
gap membership cause a conflict. A successful inverse restores prior logical
positions exactly, including normalization participants; revisions, timestamps
and audit advance. Undo-created links/contexts keep IDs as tombstones, redo
reactivates them when safe. A disposition first created by a closure can become
explicitly open on undo. Use a new audit record for compensation and the same
captured work date. Re-activate original work identities when redo safely
reapplies them; retain all correction and compensation history. Never erase
history or silently recompute placement on undo. A transaction failure rolls back
all state and leaves history position unchanged. A lost commit acknowledgment
uses existing unknown-outcome handling, checks the operation ID after reconnect
and reloads. Avoid blind replay. Permanent audit and state survive restart;
session undo/redo history resets, as in Phase 1.2.

## Proposed migration 003

Schema 2 from [001](../../migrations/mariadb/001_PersistentTaskFoundation.sql)
and [002](../../migrations/mariadb/002_OrganizationalManagement.sql) is the
authority, with nine tables at the accepted checkpoint. A new migration is
required. Leave 001/002 bytes unchanged. Proposed base tables use InnoDB and
existing UTF-8 defaults. IDs below are canonical lowercase ASCII UUIDs with
binary collation, using dataset-qualified composite FKs and equivalent
referenced unique keys. All columns are required unless marked optional.
Use restrictive FK behavior. Reuse existing checked UTC millisecond and
positive revision conventions. No extra stored code or cross-schema FK.

| Object | Proposed shape and enforcement |
| --- | --- |
| Task | Signed BIGINT priority default 5 with CHECK > 0. VARBINARY(128) manualOrderKey with nonempty, binary alphabet and final byte CHECK. Final rank NOT NULL without a default. Index (datasetId, deletedAt, priority, manualOrderKey, id). Effective group is resolved through link/context state and is never cached on Task. |
| Context | PK id, unique (datasetId,id), FK datasetId → Dataset. Name VARCHAR(1024) with existing nonblank/UTF-8 byte-limit policy. VARBINARY(128) sortKey with rank CHECK. createdAt, updatedAt, optional archivedAt/deletedAt, localRevision as BIGINT with lifecycle time/revision checks. Index (datasetId, deletedAt, sortKey, id). |
| TaskContext | PK id, datasetId, taskId, contextId, createdAt, updatedAt, optional deletedAt, localRevision. Same-dataset FKs to Task and Context. All live links are primary in schema 3. Persistent generated liveTaskId equals taskId when deletedAt IS NULL and otherwise NULL. UNIQUE (datasetId,liveTaskId) enforces one live link across contexts, including archived assignments; multiple tombstones remain possible. Index (datasetId,contextId,deletedAt,taskId) for impact check. App validates live/reference state and restores old link IDs. |
| CommandRecord and LocalChange | Add optional contextId with same-dataset Context FK and history/index coverage analogous to 002. Replace target/version CHECKs to allow exactly one root among Task/Pillar/Project/Objective/Context, retaining version-1/2 legality unchanged and allowing Context only in v3. Extend CommandRecord kinds with delete/assign/reorder/work/correct/day-close/day-resume (fixed strings within existing 16 chars) with version/target checks and preserve complete/reopen task-only. Keep operation IDs, outbox FK and history immutable, and add unique (datasetId,id) to CommandRecord for same-dataset work/day audit FKs. |
| WorkEntry | PK id, datasetId, taskId FK, commandId FK, workDate DATE, timeZone VARCHAR(255) ASCII binary, recordedAt BIGINT, kind (quick/session/closure/completion/correction), optional note MEDIUMTEXT, correctsEntryId, voidedAt BIGINT and voidedByCommandId FK. Unique (datasetId,taskId,id) for a same-task correction FK. CHECK valid kind, positive timestamps, correction link only and always for correction, paired void fields and supported note/date bounds. localRevision BIGINT > 0 supports CAS for void projection. Generate liveQuickTaskId conditional on kind=quick and voidedAt IS NULL. UNIQUE (datasetId,workDate,liveQuickTaskId). Index (datasetId,taskId,workDate,recordedAt,id) for per-task and per-date history access. App validates kind-specific payload, calendar mapping and same-task audit semantics. |
| TaskDayDisposition | PK (datasetId,taskId,workDate), same-dataset Task FK, workDate DATE, timeZone VARCHAR(255) ASCII binary, dayStatus (open/closed), optional closureCommandId and required lastCommandId with same-dataset CommandRecord FKs, updatedAt and localRevision BIGINT > 0. CHECK closure ID exists iff closed. Index (datasetId,workDate,dayStatus,taskId). App validates audit target/date and lifecycle transition. No occurrence, allocation or schedule column or FK. |

Composite FK supporting indexes that are implicit in MariaDB should be included
in the Stage A manifest or declared explicitly where naming/fingerprint
stability requires it. Use indexed generated nullable keys to allow tombstone
history. Probe their exact expression syntax, CHECK evaluation and FK/index
metadata on MariaDB 10.5.29 and 13.0.2 with QMYSQL, including strict mode,
zero-date rejection, binary parameter binding and Windows identifier casing.
Use application validation as well as CHECKs for supported date range and real
calendar dates. No compatibility result for 003 is established.

### Ordering, preconditions and interruption

1. Extend the resource, CLI preview and runner/fingerprint/shape dispatch.
   Reuse existing MigrationAttempt with version 3. The current verifier
   hardcodes a single completed attempt row for 002 and max version 2; revise
   it to validate both attempts on schema 3 and reject any stray or incomplete
   attempt on earlier schemas. Retain old verification paths for test/recovery.
   Runtime verifies the exact supported version and never migrates on startup.
2. Verify named schema/server/account/dataset, narrow grants, CHECK/FK
   enforcement, exact completed schema-2 checksums and fingerprint, completed
   attempt 2, no attempt 3 or unexpected objects, verified sealed backup and
   pre-upgrade exports. Take migration and mutation locks in existing order.
   Quiesce writers and take a canonical digest of all nine original table
   projections, expanded beyond the existing schema-1 digest. Include 002
   ownership, organizational rows, history payloads and original journal rows.
3. Reserve attempt 3 before DDL, using logical milestone 1 for journal
   bootstrap since lastCompletedStep is constrained positive. Execute numbered
   DDL and a separate transactional deterministic rank backfill. Create Context
   and TaskContext, add Task priority and temporarily nullable rank, populate
   and verify ranks, make rank required with CHECK/index, extend audit tables,
   then create WorkEntry and TaskDayDisposition. Record exact statements,
   journal ordinals, expected catalog shape and privileges in A. Never mark a DDL
   acknowledgment as proof that an unacknowledged statement had no effect.
   Existing task metadata, owner/lifecycle values, v1/v2 audit and revisions
   are copied unchanged in original projections. No seeded contexts, work or days.
4. Compare pre/post projections by stable ID and raw history bytes, with
   null-safe encodings and row counts. Validate canonical assigned ranks,
   absence of unwanted data or schema objects, FK/index metadata and expected
   catalog. Write SchemaMigration 3 with checksum and fingerprint and mark
   attempt 3 complete in one metadata transaction. Verify version 3 in a
   fresh connection. Keep runtime credentials constrained by table/operation and
   separately review migration, diagnostic, test and restore accounts.
5. DDL implicitly commits. SQL rollback covers active DML only. On partial
   execution or acknowledgment loss, stop writers, preserve artifacts and
   record identity, checksum, last known step, actual DDL, digest and error.
   Do not replay or patch journal rows automatically. A recovery proposal
   compares actual state to the exact step manifest. Preferred recovery for a
   failed upgrade is validated restore into a new separately approved schema
   from the sealed schema-2 backup, retaining the failed schema. Resume or
   cutover requires specific review. Return to schema 2 after schema-3 writes
   requires a separate reconciliation plan for post-upgrade data. There is no
   automated down migration.

### Example verification queries for the future migration

Run only against the separately approved target. Bind placeholders to known
identities in verification tooling. Compare full catalog and exported
projections to the manifest and pre-upgrade evidence. Empty violation results
alone are insufficient.

```sql
SELECT VERSION(), DATABASE(), CURRENT_USER(),
       @@foreign_key_checks, @@check_constraint_checks;
SELECT version, checksum, schemaFingerprint, appliedAt
  FROM SchemaMigration ORDER BY version;
SELECT version, checksum, baselineFingerprint, startedAt, completedAt,
       lastCompletedStep, attemptState FROM MigrationAttempt ORDER BY version;
SELECT TABLE_NAME, ENGINE FROM information_schema.TABLES
  WHERE TABLE_SCHEMA = DATABASE() ORDER BY TABLE_NAME;
SHOW CREATE TABLE Task;
SHOW CREATE TABLE TaskContext;
SHOW CREATE TABLE WorkEntry;
SHOW CREATE TABLE TaskDayDisposition;
SELECT id FROM Task WHERE datasetId = ? AND
  (priority <> 5 OR manualOrderKey IS NULL OR
   OCTET_LENGTH(manualOrderKey) NOT BETWEEN 1 AND 128 OR
   manualOrderKey REGEXP '[^01]' OR RIGHT(manualOrderKey, 1) <> BINARY '1');
SELECT manualOrderKey, COUNT(*) FROM Task
  WHERE datasetId = ? GROUP BY manualOrderKey HAVING COUNT(*) > 1;
SELECT id, HEX(manualOrderKey) FROM Task
  WHERE datasetId = ? ORDER BY manualOrderKey, id;
SELECT (SELECT COUNT(*) FROM Context) AS contextRows,
       (SELECT COUNT(*) FROM TaskContext) AS assignmentRows,
       (SELECT COUNT(*) FROM WorkEntry) AS workRows,
       (SELECT COUNT(*) FROM TaskDayDisposition) AS dayRows;
```

Those data checks describe the immediate post-upgrade baseline. Runtime
checks additionally verify live-link uniqueness, no live links to tombstoned
contexts or tasks, quick-marker uniqueness, FK integrity, day/closure and
work/correction audit integrity. Stage A must enumerate the exact SQL for
complete inspection and the per-step fault cases. Before shipping include
matching [Mermaid schema](../diagrams/DatabaseSchema.md) and
[standalone SVG](../diagrams/DatabaseSchema.svg), review the generator and
schema tests for their 001/002 assumptions, and inspect diagram renderings.
The existing schema-2 tools and acceptance evidence remain versioned.

## Stage sequence and checkpoints

The proposed A–G sequence stands with one dependency adjustment. The rank
primitive and migration foundation must land with B because context order
already uses it and assignment changes the task's ordering partition. C then
delivers priority editing and completes task-rank behavior. Use one source
migration manifest for the final phase. Incorporate its complete reviewed DDL
before any shared or non-disposable application. B–F may develop against
in-memory fixtures and use separately approved fresh isolated schemas once the
manifest is frozen. If an approved isolated rehearsal exposes a defect, preserve
that attempt and use a new explicitly approved target for a revised candidate.
Never rewrite a migration already applied to a retained deployment.

| Stage | Proposed work and exit evidence |
| --- | --- |
| A — Architecture and migration specification | Resolve product decisions below. Deliver exact SQL, step/shape manifest, permission matrix, token arithmetic vectors and grid pseudocode, receipt and topology preconditions, timezone and Qt model checks, isolated fixture and recovery procedure. No feature implementation or database writes under this plan approval. |
| B — Contexts and shared persistence | After A approval, add shared rank primitive, schema/codec/receipt infrastructure, context CRUD/order/archive/delete/restore, link assignment and unassigned projection. Review rank, command, model and SQL source checks before accepting. |
| C — Task priority and order | Deliver int64 editing, default capture order, deterministic task-band sort and normalization. Require arithmetic extremes, migrated UUID order, conflict and preservation coverage. |
| D — Drag and keyboard | Deliver group targets, anchor resolution, insertion line, priority preview, autoscroll and keyboard path. Require equivalent intent handling, no-write cancellation and atomic move/undo tests. |
| E — Daily work | Deliver date capture, repeatable sessions and corrections, closure/resume and lifecycle integration. Require fake-clock, migration/history compatibility, audit and restart coverage. |
| F — Cross-feature integration | Validate archive, ownership, priority, grouping, completion and daily-state permutations. Inject every persistence-step failure. Review regression coverage, conflict/error recovery and exact relevant-row changes. |
| G — Platform acceptance | Run native CentOS and UCRT64 builds/offline checks. Run approved isolated migration, write and restart/restore validations and manual Qt acceptance. Reconcile evidence with environment and distinct owner/agent provenance. Request formal acceptance. |

Every exit is a stop. Request approval of evidence and explicit authorization
for the next stage. An isolated write, privilege change, restore, migration or
live deployment remains a separate action with its own approval and exact
target. Future files would include migration/runner/codec/schema diagrams,
domain/application/Qt adapters, tests and validation instructions. Exact
inventories and reports belong to their respective stage.

### Owner decisions for A

1. Confirm signed-64-bit priority, binary dyadic tokens, 128-byte ceiling,
   64-existing-row normalization bound, deterministic legacy seeding and
   rejection when bounded normalization cannot produce the exact position.
2. Apply the owner-approved link removal for every drop into No context.
   Confirm archive/restore merging policy, context-name duplication and
   direct assignment defaults. Confirm which hidden rows qualify as gap
   boundaries and what the collapsed and visually empty targets mean.
3. Confirm a device-initialized persisted calendar zone, per-command date
   capture and confirmation policy. Decide closed-today presentation, history
   correction across a closed day, session timing input, reopen on a still
   closed day and whether/how to show legacy completion history alongside new
   completion events.
4. Confirm the multi-record receipt and task root revision/timestamp policy.
   Confirm gaps are revalidated and stale compensations rejected. Approve
   migration 003 architecture for specification only. Later approve the exact
   SQL and isolated schema names, hosts, account grants and test procedures.

## Validation matrix

All proposed tests are pending. Use explicit IDs, deterministic token vectors,
controlled clocks and synchronization barriers for concurrent clients. Keep
ordinary build/CTest database-free with all DB opt-ins disabled and a fresh
build cache. Extend the documented [build procedure](../Building.md) and
[Phase 1.2 testing approach](../TestingPhase1.2.md) with Phase 1.3 commands.
Retain timestamps, source revision, tool versions, checksums and redacted
outputs. Have validation scripts invoke [environment reporting](../../scripts/environment-report.sh)
with saved server metadata provenance. Use UTF-8/LF fixture protocols and cover
native CRLF and inherited MSYS2 state where applicable.

| Test type | Coverage and required observations |
| --- | --- |
| Domain/unit | Alphabet, trailing zeros, prefix ordering, sentinels, exact midpoint vectors, head/middle/tail/empty/equal-priority placement, 128-bit exhaustion, UUID collision ties, bounded window success/refusal, seeded layout and no unrelated reorders. Positive int64 extremes, syntax/overflow errors, default 5. Archive and No context group sort, restoration, owner independence. |
| Application/unit with in-memory workspace | Context CRUD/archive/delete/restore, confirmation and tombstone undo/redo, link replacement, source-anchor removal, successor/predecessor/empty priorities, direct assignment and priority edits; command receipts and dependencies. Work marker idempotence, multiple sessions, void/replacement audit, closure without schedule, resume, legacy completion and reopen, compensation across dates. Controlled instant at midnight, differing zone dates, DST transitions, zone switches and clock rollback. |
| Qt model/controller, offscreen and native | Group/header/row identity, no context last, title alternate sort, names, empty/archived groups, stable selection, completed/closed presentation, day refresh, action states, ID-based proxy mapping, preview/drop revalidation, keyboard parity, no repository call from hover/cancel/invalid drop, post-commit model position, error/undo display. |
| Isolated MariaDB integration | Fresh installation and populated schema-2 upgrade with Unicode, tombstones, both ownership paths and v1/v2 history. Record shape/journal/fingerprint, original projections and restart equivalence, binary rank ordering parity, driver int64 fidelity, generated constraints, cross-dataset rejection, restricted accounts. Inject schema failures before/after each step and lost final acknowledgment. Fail writes between root, participant, audit and outbox steps, compare affected and untouched rows. Test concurrent gap/link/closure writers, lock timeout, stale anchors, stale undo, revision conflicts and restart persistence of work/day/task/context. |
| Manual native Qt acceptance | Visible line, destination/priority and exact-neighbor preview; slow/fast drag and autoscroll; keyboard and accessible focus; empty/collapsed/unassigned targets, archive/delete impact and undo, filter/alternate sort restriction. Work sessions and correction, separate closure/completion/reopen states and midnight refresh. Capture protected fixture evidence and exact pre-restart/post-restart comparisons separately from observed functionality. Recheck Phase 1.2 hierarchy, ownership, archive and legacy history. |

Future CTest runs should include TaskCoreTests, CommandServiceTests,
OrganizationRulesTests, WorkspaceCodecTests, SchemaConstraintsTests,
StorageSafetyTests, PersistentDesktopTests, OrganizationDesktopTests and
DatabaseSchemaTests alongside the new test targets, prototype tests and
offline validation fixtures. Existing integration suites have dedicated
targets and explicit reset permissions. Do not repurpose those database
names for Phase 1.3 or run their reset paths against preserved targets.

### Platform gates and isolation

- Produce clean terminal builds on CentOS Stream 9 and native Windows MSYS2
  UCRT64 using each platform's Qt/QMYSQL and supported connector. Run offline
  unit/model/fixture checks on both. A mocked Windows result is offline evidence.
- For each server family provision only a separately approved fresh, named
  schema on an approved host, with explicitly approved dedicated accounts and
  fixture data. Stage A will define exact target and guard arguments. Fail
  closed on mismatched account, database, server, dataset, grant set, nonempty
  target, migration attempts or foreign objects. Use synthetic fixtures and
  keep any approved restore target separate. No reset or implicit cleanup.
- Run migration, constraints, rolling restart and reconnect, undo/redo within
  a session and audit persistence checks on both platforms. Compare canonical
  data and DDL after restart and approved backup/restore, seal content and
  preserve raw integrity evidence. Record native GUI manual checklist,
  supported SQL behavior and any failure by stage. Establish exact commands
  and provenance before requesting execution approval.

## Risks and recovery dependencies

The largest risks are stale or invisible anchors, archive-to-No-context
membership changes, exhausted dense rank intervals, and revision handling
during multi-entity compensation. Bound normalization, guard gaps and validate
exact logical inverses. The database risks are implicit DDL commits, SQL
dialect differences, incorrect old-history decoding and changed date/rank
bytes across drivers. Rehearse partial states and byte comparisons in isolated
fixtures. The calendar risk is traveling or delayed commands silently moving
work to another day. Keep original date intent in command and audit, and
explicitly test native zone resolution. A deployment or restore requires
verified backup, identity, privileges and documented recovery. A failure
blocks the affected approval gate until disposition is recorded.

## Original planning approval request (approved)

Approve this plan and authorize **Stage A specification work only**, on
`jb_20261122_ms1p3`. Review the owner decisions above and either confirm the
recommended policies or identify revisions for A. Stage A's expected changes
are reviewed specification and verification materials. Revisions are
recoverable through ordinary working-tree edits with the existing history
preserved. Its verification consists of token vectors and manifest/receipt
review, platform prerequisites and an exact isolated test proposal. SQL
execution, production source implementation, grants, database reset, backup
restore, publication and the move to B each require later explicit approval
where applicable. Before any operation requiring a database approval, record
its exact command/SQL, host/schema/account, expected mutations, verification
and partial-failure recovery in this milestone.

## Final acceptance criteria

Deliver all approved phase capabilities and retain existing Phase 1.2
behavior. Demonstrate deterministic persisted groups, exact drop semantics,
accessible alternatives, safe contextual changes and positive priority
editing. Demonstrate independent work/disposition/lifecycle and original-date
compensation. Demonstrate atomic writes, intended-row changes, stale conflict
handling and preservation of legacy rows and history through migration and
restart. Complete Linux and Windows gates, reconcile actual evidence and
limitations, and obtain explicit owner acceptance with the scope/exclusions
and any exceptions recorded. Deployment to an existing database remains
separate from acceptance.

## 2026-11-22 planning evidence and files

Agent inspection covered repository instructions, README, design documents,
Phase 1.1 and Phase 1.2 acceptance and validation records, B7.10, schema SQL,
runner/lease, application receipts, Qt presentation, CMake and test entry
points. Initial `git status --short --branch` was clean on the stated branch.
`git rev-parse HEAD` identified the baseline above.
`git hash-object migrations/mariadb/*.sql` reported
`54abc6aea7933b984883d7472afb873e9e099213` for 001 and
`4c392dc251fa04bfa50e6eb5c2e522c396613bf0` for 002 (Git blob IDs).
Only this milestone was created. No build/CTest was needed for this
documentation-only change. Database migration, tests and writes were not
executed. The dates, versions and acceptance evidence in the baseline are from
the checked-in owner record.

Documentation checks passed using `git diff --check` and a direct
`python3 -` check of the new file for UTF-8/LF, trailing whitespace, balanced
code fences, and resolvable relative Markdown links. Final
`git status --short --branch` shows only this untracked file. The two migration
blob IDs match `git rev-parse HEAD:migrations/mariadb/001_PersistentTaskFoundation.sql HEAD:migrations/mariadb/002_OrganizationalManagement.sql`.

## Stage A specification

The owner approved the A–G structure and authorized Stage A specification
only. The starting tree was clean on `jb_20261122_ms1p3` at
`dedd435d8d2ea45dedc61712ece7cdd4436bddf0`. The owner confirmed that every
drop into No context clears any live assignment and requested a focused daily
workflow. This entry specifies implementation candidates for Stage B review.
The original planning record above remains historical.

### Exact rank contract and proof

For precision p, canonical token b denotes integer b divided by 2^p, where
p is the string length. Canonical strings are exactly `[01]{0,127}1`.
Distinct dyadics have unique canonical encodings because reducing a power-of-two
denominator leaves an odd numerator. The first differing byte orders both
strings numerically, since the sum of any finite suffix is strictly less than
the weight of that differing bit. If one string is a proper prefix, its
implied zero suffix is strictly smaller than the other's nonzero suffix.
UUID byte order completes the order for equal tokens and is never used in the
arithmetic. Treat 0 and 1 as distinct arithmetic sentinels for missing bounds.

For unequal ordered bounds, pad to a common precision q. If the numerators
are a and b, use numerator a+b at q+1 bits and strip trailing zero bits
before checking length. This lies strictly between the bounds. Check
canonical validity and occupancy of the resulting token in the destination
band. Equal bounds, occupied output, or a canonical result longer than 128
bytes require normalization. For arbitrary precision up to 128 and 65 total
participants, use bounded integer arithmetic supporting up to 136 bits
(grid products can approach 66 × 2^128). A fixed array of bits or limbs
suffices, including on UCRT64. Check range before narrowing or indexing.

For normalization remove the moving row, locate the exact gap g, and enumerate
contiguous windows whose gap falls inside or at an end. Start at size 1,
require both immediate neighbors when present, and try up to 64 existing
rows. Among equal lengths try nearest to balanced left/right counts, then
lowest start index. Ignore windows that do not contain required neighbors.
Keep the exterior rows fixed; let a,b be their dyadic tokens or sentinels.
Reject a >= b for that window. For precision p from 1 to 128 let
`L = floor(a × 2^p) + 1`, `H = ceil(b × 2^p) - 1`, and `M = H - L + 1`.
These bounds exclude both exterior ranks even when exactly on-grid. Let n
include the incoming row. Choose the first p with M >= n. For k from 1 to n
assign grid integer `L + floor(k × (M+1)/(n+1)) - 1` at precision p, then
canonicalize. Proof of bounds: floor at k=1 is at least 1, floor at k=n
is at most M, and each successive floor increases by at least one because
(M+1)/(n+1) >= 1. With M=n the formula returns every available integer,
from L through H. The outer order is unchanged because every rewritten rank
is strictly inside the exterior bounds. Preserve the interior neighbors'
original tuple order with the moving task placed at g. A tied run that
straddles the window edge may require expanding the window or using a distinct
exterior interval. Preserve all exterior rows and unrelated priorities.
Order/adjacency is relative to the specified full band. Exhaustion of all
eligible windows rejects the move without writes.

| Case | Input | Expected |
| --- | --- | --- |
| Prefix and significant zero | `001, 01, 011, 1, 101, 11` | Already in byte and numeric order. `1` precedes its proper extensions. |
| Canonicality | Empty, `0`, `10`, `01 `, `12`, byte 0x80, 129 bytes | Reject each. `01` and 127 zeros followed by `1` are valid. |
| Empty, head, middle, tail | 0/1 sentinels, (0,`01`), (`01`,`1`), (`1`,1) | `1`, `001`, `011`, `11` respectively. |
| Carry and reduction | (`011`,`101`) | Numerators 3+5=8 at precision 4, yielding `1`. |
| Equal rank | Rows U1,U2 with rank `1`, U1 < U2, insert between | Normalize 2 existing plus 1, exterior 0/1, p=2, L=1, H=3, ranks `01,1,11` in U1/new/U2 order. |
| Non-grid exterior | a=1/8, b=7/8, n=2 | p=2, L=1, H=3, M=3; k=1→1, k=2→2; `01,1`. |
| Exact grid endpoints | a=1/4, b=3/4, n=3 | p=3, L=3, H=5, M=3; `011,1,101`. |
| Spare grid points | At p=4, L=2, H=10, M=9, n=3 | k=1,2,3 → 3,6,8; `0011,011,1`. This is an assignment-vector check independent of minimal-p selection. |
| Head exhaustion | First rank 127 zeros then `1` | Midpoint from 0 requires 129 bits. With no other row, normalize first and incoming to `01,1` in new/old order. |
| Bounded refusal | 131 existing equal-ranked rows at `1`, insertion between rows 65 and 66 | All 2,016 eligible windows have equal exterior ranks. Refuse without writes. |
| Seed | Four existing tasks in UUID order | p=3, canonical `001,01,011,1`. |

For initial seeding choose the smallest p with 2^p > N. n=0 performs zero
writes. A level-by-level search can compute p without overflowing machine
counts. Check capacity before any schema change if the reviewed source count
cannot fit the encoder. Native tests must compare SQL ORDER BY against
unsigned C++ order, including shorter prefixes, ties and case-sensitive
canonical UUID bytes.

### Migration 003 SQL specification

This is the exact proposed statement sequence for a verified schema 2.
Use UTF-8/LF for the eventual reviewed SQL resource. The DDL markers identify
the runner manifest. The parameterized data and journal statements are runner
operations, outside the DDL resource. Bind parameters in the order displayed.
Keep identity, checksums, source row count, locks and backup checks from the
migration plan above. Never run these statements on the accepted baseline as
part of Stage A.

#### DDL 01–03

```sql
CREATE TABLE Context (
   id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL PRIMARY KEY,
   datasetId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   name VARCHAR(1024) NOT NULL,
   sortKey VARBINARY(128) NOT NULL,
   createdAt BIGINT NOT NULL,
   updatedAt BIGINT NOT NULL,
   archivedAt BIGINT NULL,
   deletedAt BIGINT NULL,
   localRevision BIGINT NOT NULL,
   UNIQUE KEY contextIdentity (datasetId, id),
   INDEX contextOrder (datasetId, deletedAt, sortKey, id),
   CONSTRAINT contextDataset FOREIGN KEY (datasetId) REFERENCES Dataset(id),
   CONSTRAINT contextIdFormat CHECK (id REGEXP '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$'),
   CONSTRAINT contextName CHECK (OCTET_LENGTH(name) BETWEEN 1 AND 1024 AND CHAR_LENGTH(TRIM(name)) > 0),
   CONSTRAINT contextRank CHECK (HEX(sortKey) REGEXP '^(30|31)*31$'),
   CONSTRAINT contextTimes CHECK (createdAt > 0 AND updatedAt >= createdAt
      AND (archivedAt IS NULL OR archivedAt BETWEEN createdAt AND updatedAt)
      AND (deletedAt IS NULL OR deletedAt BETWEEN createdAt AND updatedAt)),
   CONSTRAINT contextRevision CHECK (localRevision > 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE TaskContext (
   id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL PRIMARY KEY,
   datasetId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   taskId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   contextId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   createdAt BIGINT NOT NULL,
   updatedAt BIGINT NOT NULL,
   deletedAt BIGINT NULL,
   localRevision BIGINT NOT NULL,
   liveTaskId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin
      GENERATED ALWAYS AS (CASE WHEN deletedAt IS NULL THEN RTRIM(taskId) ELSE NULL END) PERSISTENT,
   UNIQUE KEY taskContextOneLive (datasetId, liveTaskId),
   INDEX taskContextByTask (datasetId, taskId, deletedAt),
   INDEX taskContextByContext (datasetId, contextId, deletedAt, taskId),
   CONSTRAINT taskContextTask FOREIGN KEY (datasetId, taskId) REFERENCES Task(datasetId, id),
   CONSTRAINT taskContextContext FOREIGN KEY (datasetId, contextId) REFERENCES Context(datasetId, id),
   CONSTRAINT taskContextIdFormat CHECK (id REGEXP '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$'),
   CONSTRAINT taskContextTimes CHECK (createdAt > 0 AND updatedAt >= createdAt
      AND (deletedAt IS NULL OR deletedAt BETWEEN createdAt AND updatedAt)),
   CONSTRAINT taskContextRevision CHECK (localRevision > 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

ALTER TABLE Task
   ADD COLUMN priority BIGINT NOT NULL DEFAULT 5,
   ADD COLUMN manualOrderKey VARBINARY(128) NULL,
   ADD CONSTRAINT taskPriority CHECK (priority > 0);
```

#### Runner backfill after DDL 03

Execute under both leases and inside one DML transaction. Materialize the
ordered IDs before issuing updates on the same connection. Set only the new
rank column. Require exactly one affected row per ID and verify that the
resulting rank-ordered IDs equal the original sequence; compare all original
task columns, including revision and timestamps, through the preservation
digest. Record backfill completion in the same DML transaction. Rank bindings
use bytes. Only one dataset is supported by the existing verifier.

```sql
SELECT id FROM Task WHERE datasetId = ? ORDER BY CAST(id AS BINARY);
UPDATE Task SET manualOrderKey = ?
  WHERE datasetId = ? AND id = ? AND manualOrderKey IS NULL;
SELECT id, HEX(manualOrderKey) FROM Task
  WHERE datasetId = ? ORDER BY manualOrderKey, CAST(id AS BINARY);
```

#### DDL 04–08

```sql
ALTER TABLE Task
   MODIFY COLUMN manualOrderKey VARBINARY(128) NOT NULL,
   ADD CONSTRAINT taskManualRank CHECK (HEX(manualOrderKey) REGEXP '^(30|31)*31$'),
   ADD INDEX taskBandOrder (datasetId, deletedAt, priority, manualOrderKey, id);

ALTER TABLE CommandRecord
   ADD COLUMN contextId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL DEFAULT NULL,
   DROP CONSTRAINT commandTargetV2,
   DROP CONSTRAINT commandPayloadV2,
   DROP CONSTRAINT commandKindV2,
   ADD UNIQUE KEY commandDatasetIdentity (datasetId, id),
   ADD INDEX commandContextHistory (datasetId, contextId, resultingRevision),
   ADD CONSTRAINT commandContextV3 FOREIGN KEY (datasetId, contextId) REFERENCES Context(datasetId, id),
   ADD CONSTRAINT commandTargetV3 CHECK (
      (taskId IS NOT NULL) + (pillarId IS NOT NULL) + (projectId IS NOT NULL)
      + (objectiveId IS NOT NULL) + (contextId IS NOT NULL) = 1
      AND (payloadVersion = 3 OR contextId IS NULL)),
   ADD CONSTRAINT commandPayloadV3 CHECK (
      ((payloadVersion = 1 AND taskId IS NOT NULL) OR payloadVersion IN (2,3))
      AND resultingRevision > 0 AND JSON_VALID(afterSnapshot)
      AND (beforeSnapshot IS NULL OR JSON_VALID(beforeSnapshot))),
   ADD CONSTRAINT commandKindV3 CHECK (
      commandType IN ('create','edit','move','archive','restore','complete','reopen','undo','redo')
      OR (payloadVersion = 3 AND (
         (commandType = 'delete' AND contextId IS NOT NULL)
         OR (commandType = 'reorder' AND (taskId IS NOT NULL OR contextId IS NOT NULL))
         OR (taskId IS NOT NULL AND commandType IN
            ('assign','work','correct','day-close','day-resume')))));

ALTER TABLE LocalChange
   ADD COLUMN contextId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL DEFAULT NULL,
   DROP CONSTRAINT changeTargetV2,
   DROP CONSTRAINT changePayloadV2,
   ADD INDEX changeContext (datasetId, contextId),
   ADD CONSTRAINT changeContextV3 FOREIGN KEY (datasetId, contextId) REFERENCES Context(datasetId, id),
   ADD CONSTRAINT changeTargetV3 CHECK (
      (taskId IS NOT NULL) + (pillarId IS NOT NULL) + (projectId IS NOT NULL)
      + (objectiveId IS NOT NULL) + (contextId IS NOT NULL) = 1
      AND (payloadVersion = 3 OR contextId IS NULL)),
   ADD CONSTRAINT changePayloadV3 CHECK (
      ((payloadVersion = 1 AND taskId IS NOT NULL) OR payloadVersion IN (2,3))
      AND localRevision > 0 AND JSON_VALID(immutablePayload));

CREATE TABLE WorkEntry (
   id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL PRIMARY KEY,
   datasetId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   taskId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   commandId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   workDate DATE NOT NULL,
   timeZone VARCHAR(255) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   recordedAt BIGINT NOT NULL,
   kind VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   note MEDIUMTEXT NULL,
   correctsEntryId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL,
   voidedAt BIGINT NULL,
   voidedByCommandId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL,
   localRevision BIGINT NOT NULL,
   liveQuickTaskId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin
      GENERATED ALWAYS AS (CASE WHEN kind = 'quick' AND voidedAt IS NULL THEN RTRIM(taskId) ELSE NULL END) PERSISTENT,
   UNIQUE KEY workTaskIdentity (datasetId, taskId, id),
   UNIQUE KEY workOneQuick (datasetId, workDate, liveQuickTaskId),
   INDEX workHistory (datasetId, taskId, workDate, recordedAt, id),
   INDEX workByCommand (datasetId, commandId),
   INDEX workByCorrection (datasetId, taskId, correctsEntryId),
   INDEX workByVoidCommand (datasetId, voidedByCommandId),
   CONSTRAINT workTask FOREIGN KEY (datasetId, taskId) REFERENCES Task(datasetId, id),
   CONSTRAINT workCommand FOREIGN KEY (datasetId, commandId) REFERENCES CommandRecord(datasetId, id),
   CONSTRAINT workCorrection FOREIGN KEY (datasetId, taskId, correctsEntryId) REFERENCES WorkEntry(datasetId, taskId, id),
   CONSTRAINT workVoidCommand FOREIGN KEY (datasetId, voidedByCommandId) REFERENCES CommandRecord(datasetId, id),
   CONSTRAINT workIdFormat CHECK (id REGEXP '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$'),
   CONSTRAINT workDateZone CHECK (YEAR(workDate) BETWEEN 1000 AND 9999
      AND MONTH(workDate) BETWEEN 1 AND 12 AND DAYOFMONTH(workDate) BETWEEN 1 AND 31
      AND OCTET_LENGTH(timeZone) > 0),
   CONSTRAINT workKind CHECK (kind IN ('quick','session','closure','completion','correction')
      AND ((kind = 'correction') = (correctsEntryId IS NOT NULL))
      AND (correctsEntryId IS NULL OR correctsEntryId <> id)),
   CONSTRAINT workNote CHECK (note IS NULL OR OCTET_LENGTH(note) <= 65535),
   CONSTRAINT workTimes CHECK (recordedAt > 0
      AND ((voidedAt IS NULL) = (voidedByCommandId IS NULL))
      AND (voidedAt IS NULL OR (kind <> 'correction' AND voidedAt >= recordedAt))),
   CONSTRAINT workRevision CHECK (localRevision > 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE TaskDayDisposition (
   datasetId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   taskId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   workDate DATE NOT NULL,
   timeZone VARCHAR(255) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   dayStatus VARCHAR(8) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   closureCommandId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL,
   lastCommandId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   updatedAt BIGINT NOT NULL,
   localRevision BIGINT NOT NULL,
   PRIMARY KEY (datasetId, taskId, workDate),
   INDEX dayByStatus (datasetId, workDate, dayStatus, taskId),
   INDEX dayByClosure (datasetId, closureCommandId),
   INDEX dayByCommand (datasetId, lastCommandId),
   CONSTRAINT dayTask FOREIGN KEY (datasetId, taskId) REFERENCES Task(datasetId, id),
   CONSTRAINT dayClosure FOREIGN KEY (datasetId, closureCommandId) REFERENCES CommandRecord(datasetId, id),
   CONSTRAINT dayCommand FOREIGN KEY (datasetId, lastCommandId) REFERENCES CommandRecord(datasetId, id),
   CONSTRAINT dayDateZone CHECK (YEAR(workDate) BETWEEN 1000 AND 9999
      AND MONTH(workDate) BETWEEN 1 AND 12 AND DAYOFMONTH(workDate) BETWEEN 1 AND 31
      AND OCTET_LENGTH(timeZone) > 0),
   CONSTRAINT dayState CHECK (
      (dayStatus = 'open' AND closureCommandId IS NULL)
      OR (dayStatus = 'closed' AND closureCommandId IS NOT NULL)),
   CONSTRAINT dayRevision CHECK (updatedAt > 0 AND localRevision > 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
```

The existing commandLifecycleTargetV2 CHECK remains. No historical columns
change value. WorkEntry replaces the earlier timing-column proposal.
Use SQL DATE through ISO text binding and verify an exact round trip; reject
invalid Gregorian dates and unresolved zones in the application. Require
strict mode, NO_ZERO_DATE, NO_ZERO_IN_DATE and ALLOW_INVALID_DATES off
for new schema-3 test/runtime/migration connections, without mutating global
server settings. Confirm those mode requirements on both platforms before
adopting the connection gate. Application checks task/root/date agreement
for audit references and historical correction lineage. For wall-clock
regression, clamp projection update timestamps to required monotonic
bounds while preserving raw issue instant in the receipt.

#### Journal and failure manifest

```sql
INSERT INTO MigrationAttempt
   (version, checksum, baselineFingerprint, startedAt, completedAt, lastCompletedStep, attemptState)
   VALUES (3, ?, ?, ?, NULL, 1, 'running');
UPDATE MigrationAttempt SET lastCompletedStep = ?
   WHERE version = 3 AND checksum = ? AND attemptState = 'running' AND lastCompletedStep = ?;
INSERT INTO SchemaMigration (version, checksum, schemaFingerprint, appliedAt)
   VALUES (3, ?, ?, ?);
UPDATE MigrationAttempt SET completedAt = ?, attemptState = 'completed'
   WHERE version = 3 AND checksum = ? AND attemptState = 'running' AND lastCompletedStep = 10;
UPDATE MigrationAttempt SET attemptState = 'failed'
   WHERE version = 3 AND checksum = ? AND attemptState = 'running';
```

Bind step 2/3/4 after DDL 01/02/03, step 5 in the backfill transaction,
and step 6/7/8/9/10 after DDL 04/05/06/07/08. Acknowledgment requires exactly
one row. Check full shape and digest before the final transaction, which
inserts SchemaMigration and commits completion together. If step evidence,
backfill commit or final metadata commit is uncertain, stop and inspect in a
fresh connection. A known failure may mark a confirmed running attempt
failed. Preserve original attempt 2 and compare its complete row. Use the verified schema-2 SchemaMigration.schemaFingerprint as the
baselineFingerprint. Compute the checksum from the 003 resource bytes. An unapproved target cannot be selected via
auto-creation or an automatic upgrade chain. Version 3 on a fresh fixture
requires explicit, reviewed 001→002→003 operations.

Minimum privilege plan for separate approval. Migration requires SELECT
for all preflight and digest reads, schema-scoped CREATE/ALTER/INDEX/REFERENCES
for the manifest as supported by the target and INSERT/UPDATE restricted to
journal/backfill operations as feasible. Grant verification must prove the
exact approved procedure rather than assume privilege names imply scope.
Runtime needs SELECT on the verified workspace, INSERT/UPDATE on mutable
projections, INSERT on CommandRecord and LocalChange, and no schema changes
or physical deletion. Diagnostics remain read-only. Restore/data import
requires separate narrowly authorized identities and targets. The existing
approved schemas/accounts remain outside these requests.

#### Compatibility gates for both accepted server versions

Review the official references for
[generated columns](https://mariadb.com/docs/server/reference/sql-statements/data-definition/create/generated-columns),
[ALTER TABLE](https://mariadb.com/docs/server/reference/sql-statements/data-definition/alter/alter-table)
and [SQL mode](https://mariadb.com/docs/server/server-management/variables-and-modes/sql_mode).
These document SQL features and mode dependencies. Native execution on the
specific server builds remains a pending acceptance gate.

Use independent, owner-approved synthetic fixtures on MariaDB 10.5.29
(CentOS) and 13.0.2 (Windows). Fail closed until host/port, expected
database/dataset and authenticated identities are fixed in a separate
execution approval. Run these read-only diagnostics on that approved target
before and after the approved migration:

```sql
SELECT VERSION(), DATABASE(), CURRENT_USER(), @@SESSION.sql_mode,
       @@SESSION.foreign_key_checks, @@SESSION.check_constraint_checks,
       @@lower_case_table_names, @@SESSION.time_zone;
SELECT TABLE_NAME, COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT,
       EXTRA, GENERATION_EXPRESSION, COLLATION_NAME
  FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE() ORDER BY TABLE_NAME, ORDINAL_POSITION;
SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME
  FROM information_schema.STATISTICS
  WHERE TABLE_SCHEMA = DATABASE() ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX;
SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION,
       REFERENCED_TABLE_SCHEMA, REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME
  FROM information_schema.KEY_COLUMN_USAGE
  WHERE CONSTRAINT_SCHEMA = DATABASE()
  ORDER BY TABLE_NAME, CONSTRAINT_NAME, ORDINAL_POSITION;
```

Require 13 tables after upgrade, exact registered columns, defaults, CHECK
bodies, generated expressions and all key parts, and no unexpected stored
code. Verify raw SHOW CREATE output on both builds and normalize only
approved identifier casing for the version fingerprint. Do not require
server-rendered DDL bytes to match across different servers. Verify source
001/002 checksums and preservation of each original projection separately.
Update the verifier for generated expressions and composite PK order, the
diagram generator, Mermaid and SVG under later source authorization.

| Probe | Required outcome on both versions |
| --- | --- |
| SQL and QMYSQL mode | Exact DDL succeeds on verified schema 2 with enforced FK/CHECKs and approved strict session modes. Named CHECK replacement preserves existing v1/v2 rows and rejects invalid target/version/JSON inputs. Confirm CREATE/ALTER grants and SELECT-only catalog visibility with actual role identities. |
| Rank and priorities | Exact QByteArray round trip and DATE/decimal binding through QMYSQL. Test `00`, `1010`, `1 `, NUL, oversized rank, 0 and negative priority, signed maximum and overflow. SQL rank order agrees with C++ order. |
| Generated uniqueness | A second live TaskContext or quick marker for the same key fails; multiple tombstones or voided entries succeed. Replacement and undo restore uniqueness within one transaction. A different date and a different task remain independent. |
| FK and calendar | Reject wrong dataset and wrong-task correction links, nonexistent audit references and inconsistent closure states. Reject zero date, zero month/day, February 30 and a non-leap February 29. Round-trip leap day and local ISO date with different server/session time zones. |
| Fault and preservation | Test empty and populated schema-2 fixtures. Interrupt between each of journal steps 1–10 and after issuing DDL but before its acknowledgment. Probe backfill rollback, failed/unknown final commit, fresh verification and refusal to replay a partial attempt. Compare exact old projections and protected rows. |
| Cross-platform tooling | Exercise native Qt SQL drivers and server identifier casing. Validate LF migration resource checksum, CRLF tool output, UTF-8 notes and Windows paths with spaces. Test separate native backup/restore/restart on approved targets and retain exact commands and artifacts. |

### Receipt and concurrency contract for Stage B review

A version-3 operation uses one root and one operation ID. Store audit and
outbox within the same InnoDB transaction. During the schema-3 cutover,
write new commands in v3 and retain v1/v2 decoding and serialized bytes.
This table defines the logical wire fields. JSON uses explicit null,
canonical UUIDs, decimal strings for all int64 fields, ISO date strings
and ASCII rank strings. Keys and patches are emitted in stable order.

| Section | Required content |
| --- | --- |
| Header | Version, dataset ID, command ID, operation ID, root typed key, kind, UTC issue and recorded instants, resulting root revision, optional prior compensates/reapplies command IDs. The existing typed SQL root columns and revision agree with the payload. |
| Intent | For moves, source/destination effective group (No context tagged), source/resulting priority, old/new live assignment identity and rank, selected visible anchors, preview disposition for a removed link, view eligibility policy, and placement kind. For days/work, original date, zone and capture instant, affected marker/disposition/correction IDs, intent and lifecycle expectations. |
| Patches | A unique typed key for each mutable projection, before/after revision or absence, field mask, and exact logical before/after values. Keys are entity UUIDs for Task, Context, TaskContext and WorkEntry; the disposition key is task/date scoped by the dataset. Include unchanged root logical values when a child-only operation advances the root. Exclude derived generated columns. |
| Guards | Before/after typed row and revision witnesses, positive/negative membership predicates and ordered topology slices. Capture context visibility/revision, link identity/revision, bands, real and visible anchors, normalization participants and exterior boundaries. Record absence of link, marker or disposition where correctness depends on it. |
| Storage | CommandRecord beforeSnapshot stores before-side values/guards and afterSnapshot the full canonical envelope. LocalChange carries the same complete envelope. The adapter rejects mismatch between envelope and SQL metadata. Compensation and reapplication have their own receipts. |

A topology slice identifies a context/priority partition, the ordered IDs and
rank tuples in the guarded interval, and the immediate exterior witnesses.
Mark a missing end explicitly as head/tail, or both for an empty band.
Record source and destination witnesses with the moving row excluded when
resolving anchors, and store the before and after projected slices including
participants for compensation. Overlapping slices are evaluated as one
consistent simulated plan. Validation includes membership projected through
live links and context visibility. An insertion in a guarded gap, a phantom
in a formerly empty partition, and a changed context/link revision invalidate
the witness. Changes outside the guarded regions do not conflict solely
because they happened within the workspace.

1. A view creates a stable-ID intent from one coherent workspace generation.
   Capture source, visible anchor IDs/revisions and full-band gap witnesses.
   Recompute preview after view-mode changes. Dropping into No context
   explicitly previews clearing any assignment, including archived links.
   Pointer and keyboard paths use the same preparation path.
2. Acquire the existing database mutation lease before starting a transaction.
   Reload committed workspace state, revalidate the view predicate against
   fresh state, remove the moving row conceptually and resolve anchors.
   Check the exact previewed neighbor/priority and link removal. Reject stale
   observations rather than moving to a newly inferred gap. Compare context
   deletion's exact confirmed impact set, including link revisions.
3. Prepare and validate all patches as one simulated state. Enforce positive
   revisions and checked increment with no overflow. CAS-update the root,
   insert the immutable CommandRecord, then write child patches in FK-safe
   order. Clear/tombstone a previous live link or void a live quick marker
   before installing its replacement. Insert corrections after the referred
   row exists. For multi-task normalization use deterministic key order and
   retain untouched priorities. Insert LocalChange last and commit once.
4. Undo/redo uses the expected receipt side, normalizes no positions, and
   rechecks the same topology and predicate rules. Compare exact masked
   values and effective membership. Infer expected revisions from the
   original side plus a **verified** session revision chain per key. A chain
   edge is accepted only when the committed receipt's actual before revision
   equals the prior known revision and logical preconditions match. On any
   discontinuity invalidate older dependent history for that key. A later
   successful command that merely observed an external edit must not mask
   it. Validate the hypothetical result against the opposite side's logical
   placement, using new revisions for changed rows. Restore order and links
   atomically, reporting a conflict instead of relocating a stale task.
5. Undo/redo of day/work commands preserves original date and zone.
   Append correction audit for each void or reactivation and keep immutable
   work payloads. A replacement session is another identified row. On inverse
   operations reapply void projections by CAS and preserve correction rows.
   For day redo use the original logical closure identity as provenance and
   the new command as lastCommandId. Compensation restores logical fields
   exactly, with advanced revision, timestamps and audit stamps. The visible
   current day changes only when it matches the affected date or task lifecycle
   changed. Session history moves after a confirmed commit only.
6. Two supported writers serialize under the lease. A second command
   against an old gap, marker or closure fails its guard or returns a
   verified idempotent no-op for the same logical intent, never duplicate
   write. On lost commit acknowledgment clear tentative session state,
   reconnect and check operation ID against both CommandRecord and LocalChange.
   Report observed outcome, refresh authoritative state and require fresh
   intent before retry. Never infer rollback from timeout or connection loss.

Required receipt fixtures include a cross-context move with normalization
and exact inverse; drop into No context while already effectively unassigned
with an archived FK; empty-gap races; a newly completed or archived anchor;
undo with an inserted intervening row; a local revision chain with an
external edit in its middle; correction redo after a different quick marker
asserts uniqueness; and commit loss with the operation observed or absent
after reconnect. Fault injection after each CAS/audit/outbox boundary must
leave the entire pre-state and session cursor intact for known rollback.

### Daily presentation boundary and remaining product decisions

The Stage A design retains Worked Today and Completed controls, Done for Today / Resume
Today actions and a simple task history with Add work entry and Correct entry.
An additional entry has the captured date and optional note. A correction
voids an entry and optionally replaces it on that same date. Correcting a
completion event goes through lifecycle commands. Timers, session intervals,
duration, date editing, reporting and schedule integration are deferred.
A checked indicator with remaining sessions after quick-marker removal points
to history. A closed day requires Resume for adding new work. The existing
permission rules for archived organizational branches keep applying.

| Decision for owner review | Recommended default and reason |
| --- | --- |
| Rank storage and local bound | Adopt the proven dyadic scheme, signed int64 priority, 128 bytes and at most 64 existing rows per repair. Rare dense failures are explicit. Seed old rows by UUID to avoid a giant initial collision set. |
| Archived and hidden targets | Preserve keys on archive/restore and use UUID ties. Include all non-tombstoned tasks in a full effective band, use exact nearest hidden neighbors, and append in the retained priority band for visually empty groups. Collapsed headers mean end. This keeps existing ranks meaningful. Link removal on every No context drop is already approved. |
| Context naming and direct moves | Allow duplicate display names with stable-ID disambiguation in pickers and preview. Append direct context/priority changes to their new band because these commands supply no anchor. |
| Calendar setting | Persist a device-initialized per-user IANA zone, verify Qt mappings on native builds, require a valid explicit choice if unresolved, and confirm original intent for dialogs crossing midnight or changing zones. Day state identities stay task/date. |
| Closed-day presentation and correction | Default to canonical order with a closed badge and optional closed-only view. Reopen preserves day closure; Resume removes it. Permit correcting prior work even if the day is closed, including removing its last live work marker. Disposition and work answer different questions. |
| Legacy completions | Retain and display historical complete/reopen events from CommandRecord when requested, with legacy timestamps and no fabricated local dates. New lifecycle changes use WorkEntry completion events. |
| Receipt root and mode policy | Advance the Task revision/timestamp on child commands to preserve compatibility with the existing audit root model. Require checked session date modes for schema 3, with no global setting changes. Confirm the strategy after isolated native compatibility evidence. |
| Native target and privileges | Use separate fresh fixtures and a separate restore target per server. Exact identities, grant commands, backup procedure and write execution remain later approvals. |

### 2026-11-22 Stage A evidence and limitations

- `git status --short --branch` and `git rev-parse HEAD` verified a clean
  starting tree at the baseline above. `git hash-object migrations/mariadb/*.sql`
  matched the previously recorded 001 and 002 blob IDs.
- An offline `python3 -` check used standard-library Fraction arithmetic.
  It confirmed canonical byte order for all 63 interior 1/64 grid points
  and verified 2,080 midpoint pairs over all 65 grid endpoints. It compared
  16,640 normalization cases (same bound pairs, n=1 through 8, precision
  limit 6) with explicitly enumerated interior points, including minimal
  precision and exact-fill endpoint assertions. Worked vectors covered
  non-grid and exact-grid endpoints, 128/129-bit exhaustion, and all 2,016
  eligible windows in the deterministic refusal fixture.
- A static check found eight DDL statements in the proposed order (four
  CREATE and four ALTER). This establishes statement inventory only.
  The migration SQL, QMYSQL binding, timezone mapping and compatibility
  gate remain unexecuted on both server versions.
- `git diff --check` and a direct `python3 -` UTF-8, LF, code-fence and
  relative-link check passed. Only this milestone changed. No application
  build or CTest was run for documentation-only work.

### Historical approval request — Stage A exit and Stage B entry (approved)

Approve the Stage A specification and record the recommended defaults or
your changes to the remaining product decisions. Separately authorize
Stage B source work on `jb_20261122_ms1p3`. Stage B would add the reviewed
rank primitive, context/assignment commands, schema-3 migration source,
receipt infrastructure, schema diagrams and focused offline tests. Changes
are reviewable in Git and recoverable by ordinary source edits. Verify
against the rank vectors, context and receipt fixtures, immutable migration
checksums, and updated diagrams before the Stage B exit review. Database
execution, fixture provisioning, grants, restore and subsequent stages
remain separate checkpoints. The MariaDB compatibility findings must be
recorded from separately approved isolated runs before relying on 003
for a deployment.

### Final Stage A refinement — rank length boundaries

In these vectors `0^n` means exactly n ASCII zero bytes. The caret, exponent
and concatenation notation are outside the encoding. Validate the original
byte count and alphabet before accepting a persisted or external token.
Only the arithmetic encoder strips trailing zero bits.

| Vector | Bytes | Expected |
| --- | ---: | --- |
| `0^126 + 1` | 127 | Accept. Value 2^-127. Canonical round trip returns the original bytes. |
| `0^127 + 1` | 128 | Accept. Value 2^-128. Canonical round trip returns the original bytes. |
| `0^128 + 1` | 129 | Reject on length. It represents an exact dyadic but exceeds the Phase 1.3 ceiling. |
| `0^126 + 10` | 128 | Reject as noncanonical trailing zero. It represents the same value as the 127-byte token. |
| `0^127 + 10` | 129 | Reject the input. Arithmetic results of this representation reduce to the 128-byte token before their limit is checked. |
| `0^126 + 11` | 128 | Accept. This extension of the 127-byte token sorts after it. |
| `0^127 + 0x80` | 128 | Reject a byte outside the encoding alphabet. |

The three accepted values above sort in the order `0^127 + 1`,
`0^126 + 1`, `0^126 + 11`. The midpoint from head sentinel 0 to the
127-byte token produces the valid 128-byte token. The midpoint from 0 to
the 128-byte token needs 129 bytes and enters bounded normalization. The
midpoint between `0^127 + 1` and `0^126 + 11` has raw numerator 4 at
precision 129 and reduces to the valid 127-byte token. This checks that the
arithmetic length gate runs **after** reduction. With one existing row and
no exterior rows, head normalization assigns `01` to the incoming row and
`1` to the existing row. The C++ and QMYSQL parity fixtures must use the
full bytes, including the leading zeros. SQL must reject oversized values
under the required strict session mode and reject wrong alphabets or
trailing zeros via the rank CHECK.

### WorkEntry correction through undo and redo

Consider a task with session W1 (note “draft”), recordedAt 1000 and
localRevision 1 on the captured date. Correct its note to “review” using
command C at UTC 1100. The annotations below are projection values
`(voidedAt, voidedByCommandId, localRevision)`. Each command is committed
as one transaction with the Task root revision advance, CommandRecord,
WorkEntry patches and LocalChange.

| Committed operation | W1 original session | W2 replacement session | Immutable correction rows appended |
| --- | --- | --- | --- |
| Correct C at 1100 | `(1100,C,2)` | Insert with recordedAt 1100 and `(NULL,NULL,1)` | C1 targets W1. C's receipt records W2 as its replacement. |
| Undo U at 1200 | `(NULL,NULL,3)` | `(1200,U,2)` | U1 targets W1 (reactivation), U2 targets W2 (void). U compensates C. |
| Redo R at 1300 | `(1300,R,4)` | `(NULL,NULL,3)` | R1 targets W1 (void), R2 targets W2 (reactivation). R reapplies C. |

C1, U1/U2 and R1/R2 are separate `kind='correction'` WorkEntry rows,
each with its own ID, recordedAt, the original work date/zone, its
own commandId and a same-task `correctsEntryId` referring to a session.
The receipt names the effect of each correction row. These rows keep
`voidedAt` and `voidedByCommandId` NULL permanently and are excluded from
Worked Today derivation. Undo and redo change the session projections and
append audit. They leave the original session and correction payloads,
recordedAt and commandId intact. The adapter rejects a correction of a
correction row. The same inverse pattern without W2 applies to a void-only
correction. Completion events remain under lifecycle commands.

For `workTimes`, set or clear the two void fields together in one UPDATE.
For a void, use the command's recorded UTC millis floored at the affected
entry's recordedAt, so a backward wall clock cannot violate the CHECK.
Preserve raw command timing in the audit envelope. Every live or correction
row has a NULL pair. Keep the original capture date and zone even when
compensating on a later calendar day. The command FK refers to the new audit
root when setting a void. Verify the expected pair, revisions and marker
uniqueness before any projection changes. If reactivation would conflict
with a newer live quick marker, reject the whole inverse transaction under
the receipt guard. Native tests must reject half-null pairs, void dates
earlier than recordedAt and attempts to void a correction row even with an
otherwise valid timestamp and command FK. Verify the full sequence above
including all five immutable correction rows, revision increments, surviving
work and restart persistence.

### MariaDB compatibility details and fallback gate

Run the exact candidate and the following test cases independently on MariaDB
10.5.29 and 13.0.2, through the native QMYSQL connection used by the adapter.
Future execution requires individually approved fixtures, each with a
synthetic schema-2 baseline, captured original projections, canonical UUIDs
and fresh test data. Keep CHECK/FK enforcement on. Use a new fixture for an
alternate DDL candidate and preserve rejected attempts.

| Version-sensitive area | Exact isolated test and expected result |
| --- | --- |
| CHECK replacement, DDL 05/06 | On a populated schema-2 fixture run the exact ALTERs with `DROP CONSTRAINT name` and named table-level `ADD CONSTRAINT ... CHECK` in the same ALTER. Compare original command/outbox projections byte for byte. Inspect SHOW CREATE TABLE for absence of replaced V2 names, presence of all V3 names, and retained `commandLifecycleTargetV2`. With all other columns and FKs valid, insert a V1 task root, V2 organizational root and V3 Context root (all succeed). Attempt zero/two root IDs, V2 Context root, V2 `day-close`, V3 Context `complete`, payloadVersion 4 and invalid JSON (each fails). For LocalChange use a known existing operationId from a matching test command with no outbox row yet, so the unique/FK constraints cannot explain a failed CHECK. |
| Indexed generated columns, DDL 02/07 | Inspect COLUMN_TYPE, EXTRA, GENERATION_EXPRESSION and index parts. In approved strict sessions with `PAD_CHAR_TO_FULL_LENGTH` off and on, test the same canonical full UUID and conditions used by the `CASE ... RTRIM(taskId) ... END` expressions. The generated value is byte-identical in both modes for a live link/quick marker and NULL for its tombstone/void. With valid other fields, issue a second active link for the same task in a different Context and a second live quick marker for the same task/date. Each fails on its named unique key. Tombstone/void the first, retry the second (succeeds), then attempt reactivation of the first (fails). Repeat a void/replacement and roll back, verifying both rows against pre-state. Two independent history rows with NULL generated key and same unique prefix coexist. A quick marker on another date or task succeeds. |
| Composite FKs, DDL 02/05/06/07/08 | Verify ordered child and parent key parts in KEY_COLUMN_USAGE, explicit parent unique keys and child BTREE left prefixes in STATISTICS, matching ASCII collation/length and InnoDB engine. Verify constraint names remain unique across the schema for 10.5. Using synthetic tasks T1/T2 in dataset D1 and independently seeded dataset D2 in a dedicated constraint-only fixture, attempt (D1,T1, Context from D2), a WorkEntry from D1 referencing a D2 command, a correction for T1 targeting an existing T2 entry, and a disposition referencing a D2 task with D1. Each fails on the intended FK. Repeat with correctly scoped identities (succeeds). The synthetic D2 case tests raw constraints in isolation from the single-dataset application verifier. Verify nullable optional references succeed when their CHECK allows NULL. Verify restrictive delete using an otherwise removable synthetic parent with a referencing child, under separate explicit destructive-test authorization only. |
| Driver and catalog behavior | Bind the exact rank boundary bytes and date values from this specification using prepared parameters. Inspect lengths, HEX and order on successful reads. Reject 129-byte tokens under strict mode. Compare every original projection and the complete structural manifest. Check narrow SELECT-only visibility separately. Record each native diagnostic, vendor code/SQLSTATE when rejected and observed row state. |

Expected failures must identify the named constraint or demonstrate by
controlled single-field substitution that the intended constraint rejects the
row. Roll back probe transactions. For migration tests, preserve the journal
and the actual schema separately because DDL can commit independently.
Reference MariaDB's documentation for
[CHECK syntax](https://mariadb.com/docs/server/reference/sql-statements/data-definition/alter/alter-table),
[indexed generated columns and mode consistency](https://mariadb.com/docs/server/reference/sql-statements/data-definition/create/generated-columns)
and [foreign-key restrictions](https://mariadb.com/docs/server/ha-and-performance/optimization-and-tuning/optimization-and-indexes/foreign-keys).
The proposal uses named table-level FK clauses and ordinary indexed columns
for the FK paths. Generated columns participate in uniqueness only.

There is no evidence yet that either version requires a fallback. If named
CHECK replacement fails, first distinguish a syntax rejection from a
baseline name or grant mismatch. For a confirmed combined-ALTER limitation,
a candidate for review may put ADD COLUMN and its indexes/FK in a preceding
ALTER and keep each table's CHECK drop and add together in a dedicated
ALTER, with a revised step journal and no intermediate schema accepted by
runtime. If neither form preserves enforced CHECK replacement, stop for
migration redesign. If persistent generated-key syntax or index
compatibility fails, first review a portable spelling/type with the same
semantics. A materialized nullable key is a redesign candidate only if a
reviewed null-safe CHECK enforces equality to the same CASE result
(`keyColumn <=> CASE ... END`), the same unique index remains, and every
write and compensation updates the projection atomically. If a composite
FK fails, inspect column types, collations, constraint names and key
ordering, then revise the candidate DDL to match and retest. Do not remove
the FK or substitute a single-column key. Every fallback requires review
and a new candidate checksum and target; none runs automatically or bypasses
constraints.

### Stage B approval boundary

The later owner authorization for Stage B grants **source, documentation and
database-free validation only**. It grants no migration execution,
schema/database provisioning, account or privilege changes, database-backed
tests, fixture restores or writes to preserved databases. Existing and
restored databases and their accounts, backups and evidence are excluded
from the compatibility fixtures. Each future database operation needs
its own explicit approval naming host/port, schema, account, exact SQL or
command, expected state changes, verification and recovery. Any approved
isolated test runner must fail closed before connection or writes if its
explicit authorization and target configuration are absent or mismatched.
Ordinary Stage B CTest must exclude database integration tests regardless
of inherited settings. Stage B cannot start solely from approval of this
refinement.

Refinement verification used an offline `python3 -` check with Fraction
arithmetic for seven boundary vectors and three exact midpoint cases, plus
a direct evaluation of the workTimes predicates for the three projection
states and four invalid states. UTF-8/LF, whitespace, fence and relative
Markdown link checks and `git diff --check` passed. No SQL or build/test
targets were executed. No additional architectural blocker was found in
the documentation review. Native DDL compatibility is still an isolated
execution gate, and Stage B source work still requires owner approval.

## Stage B — Contexts and shared persistence

**2026-11-23 status — Source delivered for owner review.** The owner accepted
Stage A and its recommended defaults and authorized Stage B on
`jb_20261122_ms1p3`. Stage B began from a clean tree at
`af3591e483bd91c4748392f879fbb1b7158c0978`. The original planning and
Stage A records above remain historical. No MariaDB connection, SQL
execution, migration, privilege change, provisioning or database-backed
test was performed. No Git publication was performed.

### Implemented and deferred

- Shared C++ rank encoder, syntax validation, exact midpoint arithmetic and
  deterministic seeding. Bounded window normalization uses byte/limb arithmetic
  and rejects without writes when no valid position exists. The shared code
  serves Context order and destination/initial task ranks required in B.
- Context create, rename, button ordering, archive/restore, confirmed
  deletion, and chronological undo/redo. Archive retains links and keys.
  Deletion's transaction checks the confirmed live link set and tombstones it
  with the context. Undo reactivates the same IDs when guards still match.
- Task primary assignment sets, replaces or clears the link in place within
  the same workspace transaction as the task rank and journal. An explicit
  No context selection can clear an archived assignment. The persistent desktop
  has a Context menu, a task Set Context picker, and a Group by Context view
  using contiguous rows and the Context column. Archived assignments show in
  the tooltip or in the archive-inclusive view. Group order uses rank/UUID,
  task priority/rank/UUID, and No context last. Header rows, insertion feedback
  and drag/keyboard task reorder belong to D. Priority editing belongs to C.
- Workspace command receipts now support a root plus revisioned participant
  patches, assignment provenance, typed dependency witnesses and guarded
  placement intervals. A transaction validates before/after projections,
  persists root and audit, applies prevalidated participant CAS writes with live
  link clearing before activation, and writes the identical audit envelope to
  LocalChange. Revision history does not accept a later observed external
  revision as part of a verified session chain. Known rollback preserves the
  cursor. A schema-3 commit acknowledgment loss clears history and defers
  operation lookup until an explicit Reload makes a fresh connection and
  compares the operation's audit and outbox payloads in the same dataset.
- Source declares all approved 003 DDL, including WorkEntry and
  TaskDayDisposition. Work and day application commands and their Qt controls
  belong to E. Legacy completion/reopen remains functional through the audit
  path at this stage. Schema-2 desktop compatibility is retained for source
  regression and existing installations. It opens without applying 003.
  Schema 3 uses receipt version 3. The v1/v2 audit data and runtime decoding
  paths remain available. Full drag intent and daily receipt fields follow
  their feature stages.
- Ordinary CTest contains no live integration entries, even with either old
  DB-test cache option ON. Existing Phase 1.1/1.2 live test binaries remain
  buildable for their separate historical operator procedures. Offline test
  fixtures exercise rejection paths and mocks only.

### Source schema and migration limits

`003_ContextsAndDailyWork.sql` matches the eight Stage A DDL statements,
with SHA-256
`86e4a84969a6eb36e77682ef2b8410546dbd511607a11840c817568c294abf32`.
The runner includes version-aware catalog and journal validation, the schema-2
projection digest using SQL HEX of each old field, a transactional UUID-ordered
rank backfill and numbered journal transitions. The CLI verifies in a fresh
connection after a confirmed 003 apply. Step failure, incomplete history and
unknown commit stop and leave recovery to review. There is no automatic replay
or down migration. Migration 003 application is restricted in source to the
proposed disposable `taskcompass_phase13_test` loopback target with the same-name
migration principal and explicit expected dataset/fingerprint. An isolated
desktop session has its own proposed principal `taskcompass_phase13_app`.
No account or schema exists by implication. Backup and exact grant preflight
remain operator procedure prerequisites for future isolated approvals.
Deployment to an existing development database is outside the Stage B CLI
application path and requires separate review.

The old migrations and `SchemaManifest.json` are unchanged. Version 3 uses
`Schema3Manifest.json` so the accepted schema-2 artifact retains its checksum.
The cumulative diagram and registry were regenerated from 001–003. The
Markdown includes exact CHECK expressions, generated-column definitions,
composite keys, index order and FK scope. The standalone SVG presents fields,
keys and FK relationships and is intended for zoom in an XHTML-capable browser.
Its 13 tables, 130 columns, 32 FK edges and two logical links match the source.
The first headless render failed under the command sandbox with
`sandbox_host_linux.cc:41 ... Operation not permitted`. It succeeded through
escalated local rendering with the Chromium sandbox enabled. The full and detail
screenshots were inspected. The full diagram fit inside the overview with its
self-edges and right-most tables visible; individual fields are best inspected
at native size. No renderer package was installed.

### Changed-file inventory

- Build registration: `CMakeLists.txt`.
- Domain: new `src/domain/{Context,FractionalRank}.{h,cpp}`; updated
  `src/domain/Task.{h,cpp}`.
- Application: new `src/application/ContextCommand.h` and
  `ContextCommands.cpp`; updated `CommandService.{h,cpp}`,
  `CommitOutcomeUnknown.h`, `OrganizationCommand.h` and `WorkspaceRepository.h`.
- Desktop: `src/desktop/persistent/PersistentMain.cpp`,
  `PersistentTaskController.{h,cpp}`, `PersistentTaskFilter.cpp`,
  `PersistentTaskModel.{h,cpp}` and `PersistentTaskWindow.{h,cpp}`.
- Storage: `src/infrastructure/storage/DatabaseConfig.{h,cpp}`,
  `DatabaseConnection.cpp`, `MariaDbWorkspaceRepository.{h,cpp}`,
  `MigrateMain.cpp`, `MigrationRunner.{h,cpp}`, `Migrations.qrc` and
  `WorkspaceCodec.{h,cpp}`. New migration source and version-3 manifest under
  `migrations/mariadb/`.
- Tests: new `tests/ContextStageTests.cpp`; updated
  `tests/DatabaseSchemaTests.py`, `MemoryWorkspaceRepository.h` and
  `WorkspaceCodecTests.cpp`.
- Documentation: `docs/DataModel.md`,
  `docs/diagrams/{DatabaseSchema.md,DatabaseSchema.svg,GenerateDatabaseSchema.py}`
  and this milestone. Earlier milestones, accepted evidence and scripts are
  unchanged.

### Exact database compatibility work requiring separate approval

On **each** MariaDB 10.5.29 and 13.0.2 native platform, approve the actual
loopback port, proposed fresh `taskcompass_phase13_test` schema, migration
and runtime accounts, exact grants, installation of synthetic schema-2 fixtures,
and a retained pre-operation export. Separately authorize any write or restore
case and its fresh destination. Set only the reviewed per-process
`TASKCOMPASS_PHASE13_TEST_DB_*` identity and use the documented secret
mechanism. Review the environment report before the run. The following are
candidate commands with placeholders, **not executed**:

```bash
TaskCompassMigrate --phase13-test --verify --to-version 2
TaskCompassMigrate --phase13-test --apply --to-version 3 \
   --expected-dataset '<approved-dataset-uuid>' \
   --expected-baseline-fingerprint '<approved-schema-2-fingerprint>'
TaskCompassMigrate --phase13-test --verify --to-version 3
```

1. Use the Stage A compatibility SQL diagnostics to compare SHOW CREATE,
   ordered FK/index parts, generated expressions, CHECK bodies/defaults
   and exact catalog counts. Test SELECT-only visibility, account grants,
   SQL modes and name casing. Verify journal versions 1–3, attempts 2 and 3,
   003 checksum, step 10 completed, exact original projections and default
   priority/rank values in both empty and populated fixture runs.
2. Reproduce the Stage A named CHECK replacement, generated-column
   PAD_CHAR_TO_FULL_LENGTH/NULL uniqueness tests and cross-dataset/wrong-task FK
   probes with otherwise-valid fixture data. For every expected failure,
   verify the named CHECK, key or FK and exact equal before/after projections.
   Bind and read all 127/128/129-byte rank vectors, signed maximum priority,
   zero/invalid dates and valid leap days through native QMYSQL.
3. Inject failures after attempt reservation, after each DDL before
   acknowledgment, within backfill, before final metadata commit and during
   commit acknowledgment. Inspect actual DDL and journal on a fresh connection.
   Verify refusal to resume, and preserve failed fixture and artifacts.
4. Through the isolated runtime principal only, compare context-create/order,
   archived linkage, reassignment, context delete/undo/redo and task ranks
   with expected receipts and unchanged protected rows. Inject participant
   and outbox failures, run concurrent same-gap and conflicting assignments,
   verify commit recovery in both recorded/absent cases, and compare exact
   pre-/post-restart state. Native GUI, restore and end-to-end cross-platform
   acceptance remain later gates. Stage A's fallback controls apply to every
   incompatibility.

Live operation scopes, concrete grant statements and target identities are
pending separate review; these candidate commands are not approval to execute.

### Approval Requested — Stage B exit and Stage C entry

Review the Stage B source and offline evidence, then explicitly approve its
exit and separately authorize Stage C source work if accepted. No Stage C
work or database execution follows from the Stage B authorization. Any
request to run an isolated native case must specify exact host/port, schema,
account, statements, expected modifications, verification and recovery before
execution. Current compatibility risks are the two untested native MariaDB
DDL/driver combinations, the generated expression catalog spelling,
session modes, and real CAS/DDL/acknowledgment fault behavior. Native Windows
build and GUI are pending. The in-memory and offscreen results do not establish
database or Windows behavior.

### 2026-11-23 Stage B database-free validation

Commands used on CentOS Stream 9:

```bash
cmake -S . -B /tmp/taskcompass-stageb-build \
   -DTASKCOMPASS_ENABLE_DB_TESTS=ON -DTASKCOMPASS_ENABLE_PHASE12_DB_TESTS=ON
cmake --build /tmp/taskcompass-stageb-build -j 8
scripts/environment-report.sh --build-dir /tmp/taskcompass-stageb-build \
   --output-dir /tmp/taskcompass-stageb-evidence --context phase1.3-stage-b-offline-final
ctest --test-dir /tmp/taskcompass-stageb-build --show-only=json-v1
ctest --test-dir /tmp/taskcompass-stageb-build --output-on-failure -j 6
python3 docs/diagrams/GenerateDatabaseSchema.py --manifest-only
python3 docs/diagrams/GenerateDatabaseSchema.py
python3 docs/diagrams/GenerateDatabaseSchema.py --check
python3 tests/DatabaseSchemaTests.py
sha256sum migrations/mariadb/00*.sql migrations/mariadb/SchemaManifest.json
git diff --check
git status --short --branch
```

Both legacy cache options were deliberately set ON to verify that they could
not register either live suite. CMake reported them unused. The JSON CTest
inventory had 31 entries and contained neither live integration binary.
The final build succeeded with GCC 11.5.0 and Qt 6.10.3. Final CTest passed
**31/31** in 75.25 seconds, with zero failures or skips, including old
prototype/application/model/codec tests, offline script fixtures, and the
new context/rank/receipt/preview checks. The direct schema verification and
all six diagram tests passed, including the SVG's embedded constraint and
index inventory. The environment reporter preserved
`/tmp/taskcompass-stageb-evidence/environment-2026-11-23T090258.520719_0000-ad794997.json`,
build notes, and no connection-derived server metadata. Final build/CTest logs
and overview/detail screenshots are in the same temporary evidence directory.

An earlier build caught a switch-case initialization issue in the impact
guard, fixed by scoping the deletion case. The first full offline run passed
29/31. The existing codec test expected version 3 to be unsupported, and a
new test used Qt's mutable JSON lookup to check for absence. The tests now
probe unsupported version 4 and use explicit key presence. The corrected
focused run passed 4/4 before the final 31/31 result. No database-backed
suite or native Windows build was run.

Verified SHA-256 values remain
`bb901d7580398893b5c04ee263f813b9d05873eb4bae08e0daf377faf73b0ea0`
for 001,
`7de311935843e4fc04cbca74d025d9f38986a4154d70bdd29b35a2820f87b736`
for 002, and
`9d7328b58902fd135c1433e62695274e677d2e91ad02561a419323f395e76755`
for the accepted v1/v2 schema registry. The working tree contains the Stage B
inventory above. Prior accepted evidence files have no diff.

### 2026-11-23 focused Stage B acceptance review

Reviewed the pushed source at `861f112af786d5cabc5d5dc2e01a0d54f0ad8811`
on `jb_20261122_ms1p3`. The local and remote-tracking refs agreed and
the starting worktree was clean. Stage B acceptance remains pending.

| Area | Review result |
| --- | --- |
| Context atomicity | Repository acquires the mutation lease, loads fresh state in one transaction, compares the expected receipt projection, CAS-writes root and participants, then commits with audit and outbox. Participant writes clear a live link before activating its replacement. A known failure rolls back before the session cursor moves. |
| Delete, restore and assignment | Delete compares the exact confirmed live link set and tombstones context and links. Archive/restore preserves links and sort keys. Undo restores the original identifiers only if the topology and one-primary-link invariant still hold. The new fixture checks a two-link rollback and rejects the entire inverse when another session has already reassigned one task. |
| Rank and receipts | Reviewed unsigned ASCII ordering through canonical validation, head/tail sentinels, strict exterior bounds and the capped normalization window. Endpoint and touched participant guards recheck before and after slices. The existing tests exercise boundary lengths, exhausted ranks, bounded rejection, normalized undo, stale witnesses and independent bands. The unknown-commit recovery path refreshes a verified dataset and compares the correlated audit/outbox envelope. Native InnoDB fault injection remains unexecuted. |
| Schema 2 | Desktop startup and repository operations select the verified schema version. The schema-2 branch uses its existing manifest and column/target shapes, performs no context reads, disables context controls and writes version-2 payloads without priority/rank fields. The review restores the exact schema-2 column metadata query by selecting generated-column metadata only for schema 3. This is source plus database-free legacy regression evidence. Live QMYSQL compatibility was not exercised. |
| Migration 003 | Desktop and repository call read-only verification. The default CLI prints migration 001. Preview of 003 returns embedded bytes with no connection. Apply requires version 3, the explicit Phase 1.3 fixture flag, reviewed UUID/fingerprint and the dedicated fixture realm. The runner rechecks the authenticated identity, schema-2 version and fingerprint before journaling. Added process tests refuse omitted fixture metadata and Phase 1.1/1.2/development targets before opening a connection. |

Corrective source changes are local. The context picker maps the selected
row directly to a context ID rather than resolving the dialog's returned
caption with a text lookup. If a literal context name matches a generated
disambiguation caption, the dialog displays distinct ID-qualified choices.
The offscreen regression selects that literal context and verifies the
resulting assignment. Other changes are the schema-2 catalog query isolation,
migration CLI rejection cases and multi-record deletion/inverse tests.

Changed files for this review are
`src/desktop/persistent/PersistentTaskWindow.cpp`,
`src/infrastructure/storage/MigrationRunner.cpp`,
`tests/ContextStageTests.cpp`, `tests/OrganizationDesktopTests.cpp` and
this milestone. No commit or push was performed.

#### Preservation evidence

```bash
git rev-parse HEAD origin/jb_20261122_ms1p3
git diff --exit-code 7849827 HEAD -- \
   migrations/mariadb/001_PersistentTaskFoundation.sql \
   migrations/mariadb/002_OrganizationalManagement.sql \
   migrations/mariadb/SchemaManifest.json \
   docs/milestones/Phase1.1.md docs/milestones/Phase1.2.md 'docs/Phase1.2*' \
   scripts/phase11 scripts/phase12
sha256sum migrations/mariadb/00{1,2}*.sql migrations/mariadb/SchemaManifest.json
python3 docs/diagrams/GenerateDatabaseSchema.py --check
git diff --check
```

Both refs showed the pushed revision above. The preservation diff was empty.
The SHA-256 values matched the hashes recorded in the Stage B validation above.
The diagram check verified 13 tables, 130 columns, 32 SQL FKs and two logical
references. Affected historical evidence, migrations and registry also have
no working-tree diff. No SQL or database connection was attempted.

#### Windows MSYS2 UCRT64 gate

Native execution is pending access to Windows. `uname -a` identified this
session as CentOS Linux. `vmrun list` showed zero running VMs. A Windows 10
VM is present but powered off. The historical host `feldria-w10` did not
resolve in the current environment and has no configured SSH guest access
here. No VM boot, guest login or network database access was attempted.
Linux and mocked Windows cases are recorded as offline evidence only.

For the native run, use a separate Windows checkout containing the pushed
revision and the same review corrections. The source/test-only review patch
is `/tmp/taskcompass-stageb-review.patch` on the Linux host, SHA-256
`568ae64baa9dea5d4bdba6d72e4c21b8a2010af52b81497a872551fcc86e65cf`. In an isolated
checkout at the exact pushed revision, check and apply that patch if it is
not already present. Do not overwrite an existing checkout. Then run this
block from native UCRT64 Bash:

```bash
set -euo pipefail
test "${MSYSTEM:-}" = UCRT64
git rev-parse HEAD
git status --short --branch
git diff --check
for key in "${!TASKCOMPASS_@}"; do unset "$key"; done
unset MSYS2_ARG_CONV_EXCL MSYS2_ENV_CONV_EXCL
buildDir=$(cygpath -m "$(mktemp -d)")
cmake -S . -B "$buildDir" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
   -DTASKCOMPASS_ENABLE_DB_TESTS=ON -DTASKCOMPASS_ENABLE_PHASE12_DB_TESTS=ON
cmake --build "$buildDir"
bash scripts/environment-report.sh --build-dir "$buildDir" \
   --output-dir "$buildDir/evidence" --context stage-b-native-ucrt64
ctest --test-dir "$buildDir" --show-only=json-v1 > "$buildDir/evidence/ctest-inventory.json"
python - "$buildDir/evidence/ctest-inventory.json" <<'PYTHON'
import json
from pathlib import Path
import sys
names = {item["name"] for item in json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))["tests"]}
assert not names.intersection({"MariaDbIntegrationTests", "OrganizationIntegrationTests"})
assert {"ContextStageTests", "OrganizationDesktopTests", "WorkspaceCodecTests"} <= names
PYTHON
ctest --test-dir "$buildDir" --output-on-failure \
   --output-log "$buildDir/evidence/ctest.log"
```

Record the actual source revision or patch checksum, configure/build
results, native test counts, test failure messages, and the environment
report. The old cache opt-ins are intentionally set ON as a guard regression.
The CTest inventory check stops before execution if either database
integration binary appears. No database setup, service or connection is
part of this procedure. MariaDB 003 compatibility and schema-2 live runtime
testing remain separate approvals.

#### Acceptance checkpoint

The existing Stage B approval request remains open. Native UCRT64 evidence
is outstanding, so the focused review cannot establish Stage B acceptance
yet. No Stage C or database work is authorized by this review.

#### Linux rerun for the focused review

```bash
cmake -S . -B /tmp/taskcompass-stageb-review \
   -DTASKCOMPASS_ENABLE_DB_TESTS=ON -DTASKCOMPASS_ENABLE_PHASE12_DB_TESTS=ON
cmake --build /tmp/taskcompass-stageb-review -j 8
ctest --test-dir /tmp/taskcompass-stageb-review --show-only=json-v1
ctest --test-dir /tmp/taskcompass-stageb-review \
   -R 'ContextStageTests|OrganizationDesktopTests|WorkspaceCodecTests' --output-on-failure
scripts/environment-report.sh --build-dir /tmp/taskcompass-stageb-review \
   --output-dir /tmp/taskcompass-stageb-review-evidence --context stage-b-focused-review-final
ctest --test-dir /tmp/taskcompass-stageb-review --output-on-failure -j 6
git apply --reverse --check /tmp/taskcompass-stageb-review.patch
```

The initial focused selection passed 3/3. The subsequent full run before
the added two-link case passed 31/31 in 78.08 seconds. Configure reported
both obsolete DB-test options unused, and the CTest inventory had 31 entries
and neither live integration binary. The final environment report is
`/tmp/taskcompass-stageb-review-evidence/environment-2026-11-23T173207.869743_0000-6181f612.json`.
It records CentOS Stream 9, GCC 11.5.0, Qt 6.10.3 and no server query. The
review patch passes a reverse dry-run against the current working tree.

The final corrected build succeeded. Full database-free CTest passed
**31/31 in 76.02 seconds**, including the two-link checks, the context picker
regression and the pre-connection CLI refusal cases. Build and CTest logs are
`/tmp/taskcompass-stageb-review-final-build.log` and
`/tmp/taskcompass-stageb-review-final-ctest.log`. The final
`git diff --check` passed. The working tree contains only the five review
files listed above.

#### Numbered native Windows validation scripts

The owner requested numbered scripts for the pending Stage B Windows run.
From a clean native MSYS2 UCRT64 checkout that includes these scripts and the
review corrections pushed at `ac2bf59`, run in order:

```bash
bash scripts/phase13/01-build.sh
bash scripts/phase13/02-test.sh
```

Stage 01 configures and builds in a fresh ignored subdirectory under
`build-ucrt64-stage-b/`. It records the source revision, build logs and offline
environment report. Stage 02 selects that successful build automatically and
requires the same clean source revision. It records the exact CTest inventory,
rejects live integration suites by name or executable, checks that the required
offline suites are present, reports the environment, and saves the CTest log.
Both scripts print the evidence directory. Collect its configure/build logs,
environment snapshots, CTest inventory and test results. No database service,
account or credentials are requested.

Added `scripts/phase13/01-build.sh`, `scripts/phase13/02-test.sh` and their
shared preflight `scripts/phase13/stage-b-windows.sh`. Syntax was checked with
`bash -n` on each file. A temporary offline fixture explicitly mocked Windows
UCRT64 and Linux, used a path containing spaces and UTF-8, inherited a database
credential placeholder and MSYS2 path settings, and exercised successful
dispatch, missing build, failed build/stale selector, changed revision, rejected
platform, and a live executable registered under an alias. All case assertions
passed. A trailing workspace diff check in that mock shell was intercepted by
the fake Git command and returned its unsupported-command exit 80. The real
workspace check was rerun separately. Native Windows execution and Stage B
acceptance remain pending. No application code or build registration changed.

#### 2026-11-23 native Windows Stage B result

Owner-reported output from `feldria-w10` closes the outstanding native
Windows database-free validation gate. This supersedes the earlier
native-access and execution-pending notes. The owner ran these commands
from a clean checkout on `jb_20261122_ms1p3` at
`dbf63f47e5bb59283904b1d006c680446e879757`:

```bash
scripts/phase13/01-build.sh
scripts/phase13/02-test.sh
```

- Stage 01 passed. Native Windows 10 build 19045, MSYS2 UCRT64, GCC 16.2.0,
  Qt Widgets 6.12.0 and Ninja. All 96 build steps completed. CMake reported
  both legacy database-test registration options unused, as expected.
- Stage 02 passed. The inventory guard found 33 entries and neither live
  integration suite. **33 passed, zero failed or skipped, in 1653.22 seconds.**
  ContextStageTests, OrganizationDesktopTests, WorkspaceCodecTests and
  DatabaseSchemaTests passed, along with the native ACL test entries and
  Phase 1.1/1.2 offline regression fixtures.
- Both offline environment reports recorded the clean revision above and
  source SHA-256
  `91588ceae3b85d7e0eefc7bd6b2cab49c8d32f46a4d340328aaa701448cb3f3a`.
  Stage 02 reported no environment changes. The reports record that the
  MariaDB server was not queried.
- Owner-side evidence directory:
  `C:/Users/justin/repo/TaskCompass/build-ucrt64-stage-b/run.62wfXI2U/evidence`.
  Reported snapshots are
  `environment-2026-11-23T175635.834818_0000-fdd2e74d.json` and
  `environment-2026-11-23T175715.698014_0000-867abed0.json`.
  This entry records the supplied console output. The saved Windows files
  were not independently read.

Agent documentation checks used `git rev-parse HEAD`, `git diff --check`
and a direct Markdown link and fence check. Only this milestone changed in
this follow-up, so the application build and tests were not rerun.

**Approval Requested — Stage B acceptance.** Linux passed 31/31
database-free tests during the focused review, and native Windows passed
33/33 in this run. The Stage B offline evidence is ready for owner acceptance.
Migration 003 compatibility and live QMYSQL behavior remain unverified on
both MariaDB versions and require separately approved isolated tests.
Please approve Stage B and explicitly authorize Stage C if desired.
