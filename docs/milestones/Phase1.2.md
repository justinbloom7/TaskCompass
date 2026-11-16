<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Phase 1.2 — Organizational Management

**Status: Step B source implementation resumed; source/offline review delivered on 2026-11-16. Live validation and acceptance pending.**
Step A completed on 2026-11-15. Branch inspected: `jb_20261114_ms1p2`; working tree was clean.
Only this document and the roadmap were changed by Step A. Its designs/SQL were
proposals. The dated Step B reports below identify subsequent source implementation;
no SQL execution is authorized.

[Phase 1.1](Phase1.1.md) was accepted on 2026-11-14, including Windows GUI restart
with persisted data intact. Preserve its task workflows, audit/history data,
prototype, Linux/Windows evidence, private backups/exports and restored integration
databases. Do not run the old disposable tests against those restored databases.

## 1. Baseline, inspection and scope

Read AGENTS.md; Requirements, Architecture, DataModel, Interactions, Roadmap and
the Phase 1.1 status/acceptance, migration, privilege and validation reports.
Reviewed Task/TaskService/TaskRepository/CommandReceipt/TaskCommand; Qt SQL storage,
DatabaseConfig/Connection, MigrationRunner/CLI/resources and migration 001; persistent
controller/session/window/model/filter; domain, storage-safety, fake desktop,
live MariaDB tests and their reset policy; diagram generator. Accepted architecture:
standard C++20 domain/application, Qt 6 Widgets adapters, Qt SQL/MariaDB, CMake,
terminal/VIM, CentOS Stream 9 and native Windows/UCRT64 builds. No new dependencies,
frameworks, naming/style exceptions or alternate GUI architecture are proposed.

### Confirmed Phase 1.2 delivery

- Pillar/subpillar and project/subproject management with no product depth cap.
- Flat objectives, one organizational parent each, independent of future sprints.
- Create/edit/rename/move/archive/restore; parent-aware task creation/editing.
- Persisted organizational navigation and derived context; one global chronological
  session undo/redo stack for task and organization commands, durable receipts.
- Transactional validation of ownership, cycles, same dataset, archive effects,
  dependency-safe undo-create/removal, stale revisions and failed compensations.

Exclude contexts, numeric priorities/manual order/drag-and-drop, work recording,
dates, recurrence, dependencies as a task feature, planning periods/wizards,
custom fields, synchronization, mobile, bulk subtree deletion and physical purge.
Do not create future scheduling tables simply to test preserving relationships;
prove current IDs/history survive and moves do not rewrite descendant tasks.

### Exact ownership contract (confirmed, not a new decision)

| Record | Only valid organizational parent(s) | Derived context |
| --- | --- | --- |
| Pillar | No parent, or one pillar | Its pillar ancestor path. |
| Project | Exactly one pillar OR one parent project | Follow project ancestors to one pillar, then pillar ancestors. |
| Objective | Exactly one pillar OR one project | Parent path; never another objective. |
| Task | One objective OR one direct project OR neither | Objective's entire path, direct project's path, or independent. |

No direct task→pillar, TaskObjective/ProjectObjective join tables, objective nesting
or duplicated Task pillar/project fields. An objective-owned task stores only
objectiveId; directProjectId must be NULL. Independent capture remains valid.
Every parent must exist in the same dataset. Typed FKs/CHECKs enforce row-level
shape; application rules enforce non-tombstoned parents and acyclic ancestry.

## 2. Findings and conflicts requiring explicit treatment

| Finding in authoritative baseline/current source | Consequence and recommendation |
| --- | --- |
| DataModel.md still calls migration 001 proposed/unexecuted and its physical subset live-validation-pending; Architecture/Interactions contain older prototype-only statements. | These are stale descriptions superseded by the accepted milestone/actual SQL. Do not reinterpret them as absence of persistence. Correct current descriptions during approved Step B documentation, retaining historical reports. |
| Move/archive policy is labeled a recommendation in design documents. | Adopt explicit archive plus inherited effective archive and one-parent-reference subtree moves, subject to owner approval below. Do not silently make these new confirmed rules in Step A. |
| MigrationRunner embeds only 001, expects exactly five tables/one version row, fingerprints those tables, and treats any nonempty verified schema as already initialized. | It cannot upgrade populated schema 1. Incremental registry, prefix validation, latest-state fingerprints and partial-attempt detection must precede 002. |
| TaskService owns a task-only stack; TaskRepository commits only one task/receipt/change. Controller/session are task-service-specific. | Generalize this existing path to four typed targets; do not add an independent organization stack or a second audit engine. Preserve Phase 1.1 observable behavior. |
| CommandRecord and LocalChange require taskId; payloadVersion CHECK accepts only 1; commandKind excludes organization operations. | Nullable typed organization references and version-2 payloads are needed. Preserve every existing row/JSON/version-1 interpretation. |
| A single desktop worker serializes only its own connection. | It does not prevent two processes making individually legal but jointly cyclic moves. Serialize/revalidate all commands under a shared schema mutation lease. |
| Existing integration guards only accept taskcompass_integration/taskcompass_test, and cleanup only knows five tables. Both hosts retain restored evidence there. | Do not reuse/reset/relax that target. Propose a new dedicated Phase 1.2 test database/account and separate guarded executable. |
| Diagram generator is explicitly a migration-001-only reader and rejects ALTERs. | Extend it for cumulative approved DDL, do not render its current output as schema 2. Actual diagram remains schema 1 during Step A. |
| Project/objective lifecycle columns and name uniqueness remain proposals; organization deletion UI is deferred in Interactions. | Recommend no completion lifecycle UI/columns or user-facing Delete in 1.2; enforce tombstone dependency safety for undo-create. Allow duplicate names and disambiguate owners. See decision table. |

## 3. Proposed domain and command architecture

### Values and rules

Add `Pillar`, `Project`, `Objective`, `TaskOwner`, `OrganizationParent` and
`OrganizationRules` in src/domain. Keep UUIDs, UTC signed 64-bit epoch milliseconds,
createdAt/updatedAt/deletedAt/localRevision semantics consistent with Task. Add
archivedAt to organization records only; do not add Task.archivedAt just to store
an inherited result. Use Pillar.name and Project/Objective.title as documented,
optional description, trimmed nonempty UTF-8 display text (1024-byte limit) and
optional description (65535-byte limit), no NUL. No uniqueness constraint on names.

Task gains objectiveId/directProjectId in storage and a typed owner in the core.
A variant distinguishes Independent, ObjectiveOwner and DirectProjectOwner; an
optional owner-change intent means "leave unchanged" while Independent means
"explicitly remove owner". Completion/reopen must not clear ownership. Raw row
readers still reject illegal pairs, unknown types and cross-dataset references.
No UI/persistence types enter shared values. Graph snapshots/paths are derived,
not editable persisted caches.

Use iterative ancestor walks with visited typed IDs for cycle detection and
context/archive evaluation. Fetch dataset parent pointers once per transaction
or indexed batched reads; do not rely on recursive SQL/Qt/C++ call stacks or a
fixed recursive-CTE limit as the hierarchy's depth limit. Corrupt cyclic/orphaned
input fails with diagnostics; do not silently promote it to root or repair it.
Large dataset paging is a later measured optimization, not a closure-table design now.

### One transaction and one history engine

Evolve TaskService into a shared `CommandService` (task request API can remain a
thin forwarding facade if helpful during the change). Move its bounded history/
known-revision logic once; never keep parallel authoritative stacks. Extend
CommandReceipt to a bounded four-type EntitySnapshot variant and typed EntityKey
(kind + UUID; dataset is fixed by the session). OrganizationCommand expresses
create/edit/move/archive/restore; rename is an edit. TaskCommand gains owner-change
intent. No generic CRUD/ORM/plugin command framework.

Receipts retain command/operation IDs, before/after snapshots, resulting revision,
compensatesCommandId/reappliesCommandId. Version 2 additionally records typed target,
touched-field mask and validated parent/read-set identities/revisions plus any
archived-destination confirmation. A create/move/archive affects one authoritative
entity row; descendant paths/effective archive change by derivation, not fan-out
writes. One dialog task edit may atomically change title, notes and ownership.

Introduce a focused transaction/read interface in the application repository port
(e.g. WorkspaceRepository with WorkspaceTransaction). Transaction-owned lookup/
graph methods feed the same pure OrganizationRules as unit tests. The SQL adapter
starts the transaction **before** authoritative reads/validation; it commits the
prepared typed entity mutation, receipt and LocalChange together. Returning the
receipt to CommandService means commit was acknowledged. Keep SQL/binds/locking
in the adapter, business decisions in application/domain callbacks. Adapt rather
than duplicate MariaDbTaskRepository's serializers, prepared writes and fault seams.
This deliberate port evolution is needed because validating ancestors outside the
transaction then CAS-writing only the target is insufficient.

### Concurrent changes and preview validity

All schema-2 mutations, including ordinary Task edits/undo, acquire the same bounded
connection-scoped advisory lease `TaskCompassMutation:<databaseName>` before
starting the transaction or reading a graph. Reuse the existing GET_LOCK/RAII pattern;
no new mutex table, Dataset UPDATE privilege or dependency is needed. Release on
success/rollback/error; lost connection/unknown commit requires reload and history
clear as in 1.1. It is a cooperating-writer protocol, not protection against an
administrator bypassing application rules.

Under the lease, read latest committed state and revalidate target CAS plus selected
parent/read-set revisions. A stale Move impact preview or archive confirmation
must require refresh/reconfirmation. Consistent schema-2 writers serialize even
opposite A→B/B→A moves; the second observes/rejects the cycle. Migration acquires
existing migration lease then mutation lease in that order. Stop **all old clients**
before upgrade: already-open Phase 1.1 sessions do not know this new protocol.
The new runtime requires schema 2; the migration CLI can verify schema 1 for upgrade.
No schema-1 dual-write compatibility mode is proposed.

### Undo, redo and deletion safety

Keep 100-entry, commit-gated chronological session history across both task and
organization actions; no history survives restart, durable receipts do. Successful
new commands invalidate redo only after commit; cancellation/failed validation/
rollback do not advance/discard history. Unknown commit clears history and requires
reload. Never merge/delete committed audit entries.

An inverse restores exactly touched fields (including prior parent/archivedAt),
not an entire old entity image that would overwrite unrelated fields. IDs/createdAt
remain stable; updatedAt/revision advance monotonically. Revalidate ancestry,
parent availability and dependency guards for every inverse/redo. Track known
revisions by typed EntityKey and distinguish revisions written by this session
from merely observed external revisions. Own inverse writes can legitimately
advance revisions of recorded dependencies; stale external reads must not revive
old inverses. Tests must cover mixed task/parent edits, not just two task commands.

Undo-create tombstones only if no **non-tombstoned** direct children/dependent tasks
remain; archived children and completed tasks still count. Historical tombstoned
children/receipt references remain physically present and are not cascaded away.
This permits LIFO undo child-create then parent-create, and redo parent then child.
Redo must reject a tombstoned/unavailable parent. Reject physical deletion always;
there is no production purge/Delete-subtree UI in this proposal. Test the guard
explicitly rather than exposing destructive deletion to satisfy a test name.

## 4. Proposed archive and desktop behavior

- archivedAt marks only the explicitly archived record. Effective archive is own
  flag or any owning ancestor's flag, including an objective's owner path for tasks.
  Completed status and archive visibility are independent filters.
- Archive a populated parent after an impact preview; do not modify descendant
  flags/statuses/revisions/history. Restore clears only its own archivedAt. If an
  ancestor is archived, explain that it remains effectively archived and offer
  explicit ancestor restore or a legal move; no automatic ancestor/child restore.
- A move can take a subtree out of inherited archive. Moving into effective archive
  needs explicit confirmation bound to the current destination revisions. Reject
  creation under an archived owner initially; existing title/completion/reopen
  edits remain available in Show Archived without silently clearing flags.
- Preserve QMainWindow, splitter, menus/toolbars, keyboard actions, worker ownership,
  task sorting/search/completion behavior and accepted fixture prototype unchanged.
  Add Organize/context-menu actions, legal-parent pickers, impact previews and
  Show Archived; no styling overhaul or drag reordering.
- `OrganizationTreeModel` is QAbstractItemModel backed by committed graph snapshots.
  Project-oriented tree: pillars/subpillars, projects/subprojects, flat objectives.
  Objective-oriented projection lists the same objectives with ancestor context;
  no duplicate records. Sprint navigation waits for planning periods in Phase 3.
- Select pillar/project to see descendant work, objective for its tasks; retain
  All Tasks and independent/Inbox navigation. Do not implement a triage lifecycle.
  Persist expanded/selected UUIDs and view mode in existing device-local QSettings;
  records/relationships persist in MariaDB. Stable IDs restore selection after moves.
  Controller reload returns a coherent workspace snapshot (tasks plus graph).
  Ancestor changes must refresh paths/archive filters even when the raw Task
  vector is unchanged; the current task-equality-only refresh gate is insufficient.
- Global quick-capture/New Task stays independent by default; a project/objective
  context-menu New Task supplies that owner. A pillar cannot directly own a task:
  offer legal objective/project creation or independent capture instead.
- Parent-aware task editor offers Independent, Objective or Direct Project and
  clears incompatible ownership in the same transaction. Completed task edits
  retain lifecycle. Archived items remain inspectable with path/flag indicators.
- Duplicate names are allowed; pickers show type/full path and an ID suffix when
  needed to disambiguate identical paths. Deterministic display-sort ties use IDs;
  no persistent ordering/priority fields or context grouping are introduced.

## 5. Migration runner design and safety

### Why current runner cannot be reused unchanged

applyInitial only initializes empty schemas or verifies one row/version 1; verify
expects five table names and recomputes the schema-1 fingerprint against current
SHOW CREATE output. Adding tables/columns would be rejected, and the old fingerprint
cannot still equal the altered schema. Its simple semicolon splitter is explicitly
limited to 001. Existing test/reset helpers and diagrams hardcode five tables too.
Do not work around this by changing 001, updating row 1's checksum/fingerprint,
deleting history, disabling FKs/CHECKs or adopting partially modified schemas.

### Proposed numbered registry

Keep exact migration 001 bytes/checksum
`bb901d7580398893b5c04ee263f813b9d05873eb4bae08e0daf377faf73b0ea0`.
Add 002_OrganizationalManagement.sql only in approved Step B, using the SQL below.
A bounded registry has each version's resource, source checksum, expected cumulative
table/constraint description, statement boundaries and postconditions. Reject
missing/duplicate/gapped/unknown/newer versions and any mismatched recorded checksum.
Use an explicit migration command target, proposed `--apply --to-version 2`;
preview/defaults never connect, GUI never applies migrations. Preserve an explicit
schema-1 verify path for backup/upgrade preflight and legacy regression fixtures.

Before 002, verify populated version 1 exactly with the **unchanged** version-1
fingerprint algorithm, one dataset and no unexpected tables/views/stored code.
Then preserve every Task/receipt/outbox/Dataset value and row 1's metadata. Newly
added Task owner columns are NULL for existing tasks; no task reclassification or
revision/timestamp update occurs. Runtime schema 2 requires the registered table
set, latest fingerprint and consistent completed attempt metadata.

After each version, store its completed fingerprint as evidence of that version's
state. When verifying version 2, validate all source checksums/contiguous history,
but compare **only row 2's** fingerprint to current schema 2. Do not recompute row
1's obsolete shape against altered tables or rewrite its recorded fingerprint.
Fingerprint canonicalization must retain 001 byte/order/case behavior for version
1 and extend identifier normalization to all new tables for version 2. Server/DDL
rendering differences remain a conservative stop condition, not permission to
rewrite fingerprints. Cross-version/server restore portability is not promised.

MigrationAttempt is a small local-only journal for schema 2 onward, not domain
history/sync. It records version/checksum/baseline fingerprint/state/last completed
statement. Its own first CREATE is a bootstrap exception: if interrupted before
an attempt row is inserted, extra-table detection still refuses the partial state.
Start the attempt before remaining DDL; after each successful statement advance
the journal. On failure preserve journal/schema and stop; a lost connection may
leave lastCompletedStep behind actual DDL, so it is evidence, never auto-resume
permission. Complete attempt + insert SchemaMigration row 2 in one metadata
transaction only after final structural and preservation checks.

MariaDB DDL can implicitly commit; 10.5 and 13 cannot be assumed to have identical
crash/atomic-DDL behavior. An incomplete/running/failed attempt, mismatched objects,
or inconsistent completed metadata blocks runtime/upgrade. Verified already-complete
version 2 permits a no-write "already applied" result, not another DDL pass.
Avoid stored programs/DELIMITER and use reviewed statement manifests rather than
promoting the 001 splitter to an unrestricted SQL parser.

## 6. Exact proposed migration 002 SQL — not executed

The following is the proposed DDL, not a new migration file. Nine ordered DDL steps:
MigrationAttempt, Pillar, Project, Objective, Task, CommandRecord detach, CommandRecord
extend, LocalChange detach, LocalChange extend. The two detach/extend pairs avoid
combining old-name constraint removal/redefinition in one ALTER; transitional
constraint gaps exist only during stopped-writer maintenance and are journaled.
All old non-NULL IDs/payloads remain unchanged. There are nine cumulative tables.
No IF EXISTS/IF NOT EXISTS, DROP TABLE/COLUMN, cascading deletes or data backfill.
SQL syntax, constraint naming/ALTER compatibility and minimal privileges still
require approved isolated live validation on both MariaDB versions.

```sql
CREATE TABLE MigrationAttempt (
   version INT NOT NULL PRIMARY KEY,
   checksum CHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   baselineFingerprint CHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   startedAt BIGINT NOT NULL,
   completedAt BIGINT NULL,
   lastCompletedStep INT NOT NULL,
   attemptState VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   CONSTRAINT attemptVersion CHECK (version >= 2),
   CONSTRAINT attemptStep CHECK (lastCompletedStep >= 1),
   CONSTRAINT attemptStateValue CHECK (attemptState IN ('running', 'failed', 'completed')),
   CONSTRAINT attemptTimes CHECK (startedAt > 0 AND
      ((attemptState = 'completed' AND completedAt IS NOT NULL AND completedAt >= startedAt)
       OR (attemptState IN ('running', 'failed') AND completedAt IS NULL)))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE Pillar (
   id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL PRIMARY KEY,
   datasetId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   parentPillarId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL,
   name VARCHAR(1024) NOT NULL,
   description MEDIUMTEXT NULL,
   archivedAt BIGINT NULL,
   createdAt BIGINT NOT NULL,
   updatedAt BIGINT NOT NULL,
   deletedAt BIGINT NULL,
   localRevision BIGINT NOT NULL,
   UNIQUE KEY pillarDatasetIdentity (datasetId, id),
   INDEX pillarParent (datasetId, parentPillarId, deletedAt),
   CONSTRAINT pillarDataset FOREIGN KEY (datasetId) REFERENCES Dataset(id),
   CONSTRAINT pillarParentScope FOREIGN KEY (datasetId, parentPillarId) REFERENCES Pillar(datasetId, id),
   CONSTRAINT pillarIdFormat CHECK (id REGEXP '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$'),
   CONSTRAINT pillarName CHECK (OCTET_LENGTH(name) BETWEEN 1 AND 1024 AND CHAR_LENGTH(TRIM(name)) > 0),
   CONSTRAINT pillarDescription CHECK (description IS NULL OR OCTET_LENGTH(description) <= 65535),
   CONSTRAINT pillarNotSelf CHECK (parentPillarId IS NULL OR parentPillarId <> id),
   CONSTRAINT pillarTimes CHECK (createdAt > 0 AND updatedAt >= createdAt AND
      (archivedAt IS NULL OR archivedAt BETWEEN createdAt AND updatedAt) AND
      (deletedAt IS NULL OR deletedAt BETWEEN createdAt AND updatedAt)),
   CONSTRAINT pillarRevision CHECK (localRevision > 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE Project (
   id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL PRIMARY KEY,
   datasetId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   pillarId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL,
   parentProjectId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL,
   title VARCHAR(1024) NOT NULL,
   description MEDIUMTEXT NULL,
   archivedAt BIGINT NULL,
   createdAt BIGINT NOT NULL,
   updatedAt BIGINT NOT NULL,
   deletedAt BIGINT NULL,
   localRevision BIGINT NOT NULL,
   UNIQUE KEY projectDatasetIdentity (datasetId, id),
   INDEX projectPillar (datasetId, pillarId, deletedAt),
   INDEX projectParent (datasetId, parentProjectId, deletedAt),
   CONSTRAINT projectDataset FOREIGN KEY (datasetId) REFERENCES Dataset(id),
   CONSTRAINT projectPillarScope FOREIGN KEY (datasetId, pillarId) REFERENCES Pillar(datasetId, id),
   CONSTRAINT projectParentScope FOREIGN KEY (datasetId, parentProjectId) REFERENCES Project(datasetId, id),
   CONSTRAINT projectParentChoice CHECK ((pillarId IS NOT NULL) + (parentProjectId IS NOT NULL) = 1),
   CONSTRAINT projectNotSelf CHECK (parentProjectId IS NULL OR parentProjectId <> id),
   CONSTRAINT projectIdFormat CHECK (id REGEXP '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$'),
   CONSTRAINT projectTitle CHECK (OCTET_LENGTH(title) BETWEEN 1 AND 1024 AND CHAR_LENGTH(TRIM(title)) > 0),
   CONSTRAINT projectDescription CHECK (description IS NULL OR OCTET_LENGTH(description) <= 65535),
   CONSTRAINT projectTimes CHECK (createdAt > 0 AND updatedAt >= createdAt AND
      (archivedAt IS NULL OR archivedAt BETWEEN createdAt AND updatedAt) AND
      (deletedAt IS NULL OR deletedAt BETWEEN createdAt AND updatedAt)),
   CONSTRAINT projectRevision CHECK (localRevision > 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE Objective (
   id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL PRIMARY KEY,
   datasetId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   pillarId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL,
   projectId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL,
   title VARCHAR(1024) NOT NULL,
   description MEDIUMTEXT NULL,
   archivedAt BIGINT NULL,
   createdAt BIGINT NOT NULL,
   updatedAt BIGINT NOT NULL,
   deletedAt BIGINT NULL,
   localRevision BIGINT NOT NULL,
   UNIQUE KEY objectiveDatasetIdentity (datasetId, id),
   INDEX objectivePillar (datasetId, pillarId, deletedAt),
   INDEX objectiveProject (datasetId, projectId, deletedAt),
   CONSTRAINT objectiveDataset FOREIGN KEY (datasetId) REFERENCES Dataset(id),
   CONSTRAINT objectivePillarScope FOREIGN KEY (datasetId, pillarId) REFERENCES Pillar(datasetId, id),
   CONSTRAINT objectiveProjectScope FOREIGN KEY (datasetId, projectId) REFERENCES Project(datasetId, id),
   CONSTRAINT objectiveParentChoice CHECK ((pillarId IS NOT NULL) + (projectId IS NOT NULL) = 1),
   CONSTRAINT objectiveIdFormat CHECK (id REGEXP '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$'),
   CONSTRAINT objectiveTitle CHECK (OCTET_LENGTH(title) BETWEEN 1 AND 1024 AND CHAR_LENGTH(TRIM(title)) > 0),
   CONSTRAINT objectiveDescription CHECK (description IS NULL OR OCTET_LENGTH(description) <= 65535),
   CONSTRAINT objectiveTimes CHECK (createdAt > 0 AND updatedAt >= createdAt AND
      (archivedAt IS NULL OR archivedAt BETWEEN createdAt AND updatedAt) AND
      (deletedAt IS NULL OR deletedAt BETWEEN createdAt AND updatedAt)),
   CONSTRAINT objectiveRevision CHECK (localRevision > 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

ALTER TABLE Task
   ADD COLUMN objectiveId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL DEFAULT NULL,
   ADD COLUMN directProjectId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL DEFAULT NULL,
   ADD CONSTRAINT taskOwnerChoice CHECK ((objectiveId IS NOT NULL) + (directProjectId IS NOT NULL) <= 1),
   ADD INDEX taskObjective (datasetId, objectiveId, deletedAt),
   ADD INDEX taskDirectProject (datasetId, directProjectId, deletedAt),
   ADD CONSTRAINT taskObjectiveScope FOREIGN KEY (datasetId, objectiveId) REFERENCES Objective(datasetId, id),
   ADD CONSTRAINT taskDirectProjectScope FOREIGN KEY (datasetId, directProjectId) REFERENCES Project(datasetId, id);

ALTER TABLE CommandRecord
   DROP FOREIGN KEY commandTask,
   DROP CONSTRAINT commandPayload,
   DROP CONSTRAINT commandKind;

ALTER TABLE CommandRecord
   MODIFY COLUMN taskId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL,
   ADD COLUMN pillarId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL DEFAULT NULL,
   ADD COLUMN projectId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL DEFAULT NULL,
   ADD COLUMN objectiveId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL DEFAULT NULL,
   ADD CONSTRAINT commandTaskV2 FOREIGN KEY (datasetId, taskId) REFERENCES Task(datasetId, id),
   ADD CONSTRAINT commandPillarV2 FOREIGN KEY (datasetId, pillarId) REFERENCES Pillar(datasetId, id),
   ADD CONSTRAINT commandProjectV2 FOREIGN KEY (datasetId, projectId) REFERENCES Project(datasetId, id),
   ADD CONSTRAINT commandObjectiveV2 FOREIGN KEY (datasetId, objectiveId) REFERENCES Objective(datasetId, id),
   ADD CONSTRAINT commandTargetV2 CHECK ((taskId IS NOT NULL) + (pillarId IS NOT NULL) + (projectId IS NOT NULL) + (objectiveId IS NOT NULL) = 1),
   ADD CONSTRAINT commandPayloadV2 CHECK ((payloadVersion = 1 AND taskId IS NOT NULL OR payloadVersion = 2) AND resultingRevision > 0 AND JSON_VALID(afterSnapshot) AND (beforeSnapshot IS NULL OR JSON_VALID(beforeSnapshot))),
   ADD CONSTRAINT commandKindV2 CHECK (commandType IN ('create', 'edit', 'move', 'archive', 'restore', 'complete', 'reopen', 'undo', 'redo')),
   ADD CONSTRAINT commandLifecycleTargetV2 CHECK (commandType NOT IN ('complete', 'reopen') OR taskId IS NOT NULL),
   ADD INDEX commandPillarHistory (datasetId, pillarId, resultingRevision),
   ADD INDEX commandProjectHistory (datasetId, projectId, resultingRevision),
   ADD INDEX commandObjectiveHistory (datasetId, objectiveId, resultingRevision);

ALTER TABLE LocalChange
   DROP FOREIGN KEY changeTask,
   DROP CONSTRAINT changePayload;

ALTER TABLE LocalChange
   MODIFY COLUMN taskId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL,
   ADD COLUMN pillarId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL DEFAULT NULL,
   ADD COLUMN projectId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL DEFAULT NULL,
   ADD COLUMN objectiveId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL DEFAULT NULL,
   ADD CONSTRAINT changeTaskV2 FOREIGN KEY (datasetId, taskId) REFERENCES Task(datasetId, id),
   ADD CONSTRAINT changePillarV2 FOREIGN KEY (datasetId, pillarId) REFERENCES Pillar(datasetId, id),
   ADD CONSTRAINT changeProjectV2 FOREIGN KEY (datasetId, projectId) REFERENCES Project(datasetId, id),
   ADD CONSTRAINT changeObjectiveV2 FOREIGN KEY (datasetId, objectiveId) REFERENCES Objective(datasetId, id),
   ADD CONSTRAINT changeTargetV2 CHECK ((taskId IS NOT NULL) + (pillarId IS NOT NULL) + (projectId IS NOT NULL) + (objectiveId IS NOT NULL) = 1),
   ADD CONSTRAINT changePayloadV2 CHECK ((payloadVersion = 1 AND taskId IS NOT NULL OR payloadVersion = 2) AND localRevision > 0 AND JSON_VALID(immutablePayload)),
   ADD INDEX changePillar (datasetId, pillarId),
   ADD INDEX changeProject (datasetId, projectId),
   ADD INDEX changeObjective (datasetId, objectiveId);
```

Existing operationId uniqueness, changeCommand FK, acknowledgment state CHECK,
commandTaskHistory/changePending indexes, compensation/reapplication logical links
and all nonremoved 001 constraints remain. No FK has ON DELETE CASCADE. SQL cannot
enforce ancestor cycles, effective archive, live-parent availability or full
agreement of the receipt/outbox target; shared commands validate these inside the
transaction. FK/CHECK tests must distinguish these SQL limits from application rules.
Version-1 rows remain task-targeted and valid; new writes use payloadVersion=2 with
standard JSON snapshots and decimal-string 64-bit integers. Decode version 1 as an
independent task snapshot; never rewrite its immutable JSON to add ownership.
The new receipt/outbox pillarId/projectId/objectiveId columns identify the mutated
organization entity, not redundant parent context for a task; exactly one target
is present per record.

### Prepared runner metadata SQL (not part of DDL resource)

Bind every `?`; do not interpolate timestamps/checksums or derive dataset IDs anew.
After first DDL/bootstrap and before step 2:

```sql
INSERT INTO MigrationAttempt
   (version, checksum, baselineFingerprint, startedAt, completedAt, lastCompletedStep, attemptState)
   VALUES (2, ?, ?, ?, NULL, 1, 'running');
```

After each acknowledged remaining statement:

```sql
UPDATE MigrationAttempt SET lastCompletedStep=?
   WHERE version=2 AND checksum=? AND attemptState='running' AND lastCompletedStep=?;
```

Require one affected row. After DDL/structural checks and original-data comparison,
in one transaction (monotonic completion timestamp, exactly one insert/update):

```sql
INSERT INTO SchemaMigration (version, checksum, schemaFingerprint, appliedAt)
   VALUES (2, ?, ?, ?);
UPDATE MigrationAttempt SET attemptState='completed', completedAt=?
   WHERE version=2 AND checksum=? AND attemptState='running' AND lastCompletedStep=9;
```

On an observable failure, best-effort separately record failure without pretending
failed DDL rolled back:

```sql
UPDATE MigrationAttempt SET attemptState='failed'
   WHERE version=2 AND checksum=? AND attemptState='running';
```

A connection/commit loss requires inspection
of actual objects and both metadata records, not blind retry. No UPDATE of row 1,
Dataset or existing task/audit/outbox data is in the proposed migration.

## 7. Exact proposed privileges and isolated test target — later approval only

Use existing dev diagnostic/runtime/migrator accounts, after inspecting actual
privileges/roles/PUBLIC. Diagnostic SELECT-only grants remain unchanged. Runtime
never gets DDL/DELETE, and migrations never use root as the application identity.

Temporary grants to the current read-only migrator, **proposed, not executed**:

```sql
GRANT CREATE ON taskcompass_dev.Pillar TO 'taskcompass_migrate'@'127.0.0.1';
GRANT CREATE ON taskcompass_dev.Project TO 'taskcompass_migrate'@'127.0.0.1';
GRANT CREATE ON taskcompass_dev.Objective TO 'taskcompass_migrate'@'127.0.0.1';
GRANT CREATE, INSERT, UPDATE ON taskcompass_dev.MigrationAttempt TO 'taskcompass_migrate'@'127.0.0.1';
GRANT ALTER ON taskcompass_dev.Task TO 'taskcompass_migrate'@'127.0.0.1';
GRANT ALTER ON taskcompass_dev.CommandRecord TO 'taskcompass_migrate'@'127.0.0.1';
GRANT ALTER ON taskcompass_dev.LocalChange TO 'taskcompass_migrate'@'127.0.0.1';
GRANT INSERT ON taskcompass_dev.SchemaMigration TO 'taskcompass_migrate'@'127.0.0.1';
```

Existing schema-scoped SELECT covers verification. No domain DML, Dataset UPDATE,
global privileges, GRANT OPTION, DROP TABLE or permanent schema-wide DDL rights.
MariaDB documents ALTER as the general required table privilege; renaming tables
would require more, but none is proposed. Validate this minimum on 10.5.29 and
13.0.2 under real least-privilege accounts; stop and review if more is required.

After schema 2 verifies, add only:

```sql
GRANT SELECT, INSERT, UPDATE ON taskcompass_dev.Pillar TO 'taskcompass_app'@'127.0.0.1';
GRANT SELECT, INSERT, UPDATE ON taskcompass_dev.Project TO 'taskcompass_app'@'127.0.0.1';
GRANT SELECT, INSERT, UPDATE ON taskcompass_dev.Objective TO 'taskcompass_app'@'127.0.0.1';
GRANT SELECT ON taskcompass_dev.MigrationAttempt TO 'taskcompass_app'@'127.0.0.1';
REVOKE CREATE ON taskcompass_dev.Pillar FROM 'taskcompass_migrate'@'127.0.0.1';
REVOKE CREATE ON taskcompass_dev.Project FROM 'taskcompass_migrate'@'127.0.0.1';
REVOKE CREATE ON taskcompass_dev.Objective FROM 'taskcompass_migrate'@'127.0.0.1';
REVOKE CREATE, INSERT, UPDATE ON taskcompass_dev.MigrationAttempt FROM 'taskcompass_migrate'@'127.0.0.1';
REVOKE ALTER ON taskcompass_dev.Task FROM 'taskcompass_migrate'@'127.0.0.1';
REVOKE ALTER ON taskcompass_dev.CommandRecord FROM 'taskcompass_migrate'@'127.0.0.1';
REVOKE ALTER ON taskcompass_dev.LocalChange FROM 'taskcompass_migrate'@'127.0.0.1';
REVOKE INSERT ON taskcompass_dev.SchemaMigration FROM 'taskcompass_migrate'@'127.0.0.1';
```

App keeps existing Task/audit/outbox grants. Metadata verifies exact final grants,
no unexpected global/role/PUBLIC authority. No password changes or new dev accounts.
The stricter 1.1 script grant validators must remain historical/version-specific,
not be weakened to accept arbitrary extra rights. New helpers declare schema 2.

### New test isolation

Recommend `taskcompass_phase12_test` with a same-named TCP user; preserve both
restored taskcompass_integration databases/accounts completely. New provisioning:

```sql
CREATE DATABASE taskcompass_phase12_test
   CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER 'taskcompass_phase12_test'@'127.0.0.1'
   IDENTIFIED BY '<distinct-strong-phase12-test-password>';
GRANT SELECT, INSERT, UPDATE, CREATE, DROP, ALTER
   ON `taskcompass\_phase12\_test`.* TO 'taskcompass_phase12_test'@'127.0.0.1';
```

Do not execute this in Step A/B source approval. Source changes would add a specific
DatabasePurpose/test configuration (`TASKCOMPASS_PHASE12_TEST_DB_*`, explicit
isolated=1/loopback/user checks), separate `OrganizationIntegrationTests` executable
and proposed CLI `--phase12-test`. Old --integration/Phase 1.1 test guards remain
bound to their original identities, not repointed or relaxed. New live suite skips
with exit 77 without exact opt-in, and only owns objects after proof of completely
empty startup in the **new** target. Its nine-table allowlist/known FK-order cleanup
is restricted to its own run. Pre-existing/failed/restored objects stop startup;
no auto-reset/adoption. Ordinary CTest stays offline with live tests OFF by default.

## 8. Verification, backup and recovery gates

Prepare source/scripts first; then request actual target/platform-specific approval
with final SQL hash, privileges, preflight, command list and private snapshot path.
Existing restored integration data/backups are never candidates for destructive tests.

1. Stop all TaskCompass writers on the target (no MariaDB service restart needed).
   Verify normal authentication, version 1, known table set/engines/constraints,
   matching 001 checksum/fingerprint, single dataset and expected grants/roles/PUBLIC.
2. Before development changes, take a **new** private populated version-1 backup
   with the reviewed no-DROP/no-USE/no-create-db dump options. Preserve original
   artifacts. Capture exit/status/hash, UUID and deterministic legacy-column
   exports of all five tables; private Windows ACLs remain mandatory.
3. Rehearse 1→2 on separately approved isolated fixtures/clones, including archived/
   completed/tombstoned Task rows and version-1 JSON with UTF-8/NULL values. A dump
   is executable SQL: exact-file restore still has its own approval. No fixture
   run may reset or import over the retained taskcompass_integration schemas.
   The destructive suite creates its synthetic version-1 data only after empty-start
   proof in its own process. A real restored snapshot is upgraded/verified with
   the guarded CLI and read-only checks, never adopted by that destructive suite.
4. After approved dev migration, verify contiguous rows 1/2, unchanged row 1,
   completed attempt 2/step 9/checksum/baseline link, latest schema-2 fingerprint,
   expected **nine** table set, all FKs/CHECKs/indexes and no stored code.
5. Compare original legacy-column values/row counts for Dataset, Task, CommandRecord
   and LocalChange, and row 1's full SchemaMigration tuple. Task ownership is all
   NULL initially, new org tables empty. Full SELECT * hashes differ after added
   columns: compare the explicit old-column projection, not mismatched schemas.
   Preserve UUIDs, old JSON bytes, revisions/times and tombstones exactly.
6. Verify all task workflows plus organization on Linux and Windows using runtime
   grants; no CREATE/DROP runtime permission probes. After completion, take a new
   populated schema-2 dump and rehearse approved empty-target restore, checking
   schema/dataset/all **nine** tables including attempts and both history versions.
   Preserve resulting evidence, no automatic cleanup.

Partial-failure matrix: before first CREATE, schema 1 is untouched; after journal
CREATE but before row insertion, extra-table detection blocks; after attempt start
or any CREATE/ALTER, recorded step plus actual objects identify partial state;
after all DDL but before final metadata commit, missing completed row 2 blocks;
after uncertain metadata commit, inspect latest fingerprint/attempt/history before
any retry. Never infer safety from lastCompletedStep alone. All such failures stop
without automatic suffix execution, fingerprint rewriting or DROP cleanup.

No generic down migration is proposed. Old binaries cannot use schema 2. Rollback
means a separately approved recovery using the verified version-1 snapshot, with
failed state preserved first, preferably into a fresh explicitly approved recovery
target; any in-place dev restoration needs its own exact-file/destructive approval.
If schema-2 edits already occurred, first preserve them; blindly restoring version
1 would lose them. Baseline dumps alone do not prove recovery: rehearse upgrade/
restore under the minimum accounts on both versions before development execution.

## 9. Staged Step B implementation plan — all proposed

| Stage / dependency | Behavior and principal files | Tests and exit gate |
| --- | --- | --- |
| B1 — shared values/rules, after owner design approval | Add domain/Pillar.h/.cpp, Project, Objective, TaskOwner, OrganizationParent, OrganizationRules; extend Task. No live SQL. | Pure C++ ownership matrix, UTF-8/limits, iterative deep ancestry, cycles, cross-dataset, effective archive and live-child guards. Existing Task semantics still pass. Build Linux; native Windows compile/CTest before release. |
| B2 — transaction/command/history extension, after B1 | Add application/OrganizationCommand, CommandService, WorkspaceRepository/WorkspaceTransaction ports; evolve existing TaskService/TaskRepository/CommandReceipt rather than duplicate engines. Extend MemoryTaskRepository or add one shared MemoryWorkspaceRepository. | Mixed-type chronological undo/redo, failed commit/redo branch invariants, read-set staleness, own vs external revisions, parent tombstone redo, unknown-commit behavior. No Qt in core; unit tests offline. |
| B3 — migration infrastructure and 002, after B1/B2 design stabilizes | Modify storage/MigrationRunner.h/.cpp, MigrateMain.cpp, DatabaseConfig.h/.cpp, Migrations.qrc; add MigrationDefinition/002 SQL, journal/registry and version-aware structural checks. Update diagrams/DatabaseSchema.md/.svg and generator together from actual 001+002 source. | Resource/checksum/statement/identity/version-policy tests without live DB. 001 bytes/fingerprint algorithm remain unchanged. No live DDL. Narrow registry/journal policies unit-test gaps/unknown states; live behavior awaits explicit approval. |
| B4 — typed SQL repository, after B2/B3 | Evolve MariaDbTaskRepository to MariaDbWorkspaceRepository (or focused organization adapter sharing its one transaction implementation), serializer/codecs for v1/v2, schema mutation lease, graph reads, prepared CAS/receipt/outbox writes. | Memory/fake failure seams, then approved new isolated MariaDB suite: fresh 001→002 and populated upgrade, exact SQL constraints, locking races, fault injection and all Phase 1.1 task contracts on schema 2. Separate live approval required first. |
| B5 — Widgets organization workflows, after B2/B4 | Extend PersistentTaskSession/Controller/Window/Model/Filter/Main; add desktop/models/OrganizationTreeModel, dialogs/OrganizationEditDialog, OrganizationMoveDialog and parent-aware Task dialog. Keep prototype untouched. | QAbstractItemModelTester, fake-session menu/dialog/selection tests, archive/owner/filter behavior and no optimistic pre-commit UI mutation. Manual keyboard/splitter/accessibility, mixed undo and restart on both desktops. No live GUI until schema/grants approved. |
| B6 — operator tools, diagrams and isolated platform validation, after B3–B5 | Add version-specific Phase1.2 validation helpers/docs and mock tests/CMake registrations; preserve 1.1 safety behavior. Update current Architecture/DataModel descriptions with implemented facts only. | Ordinary CTest off-server; Linux 10.5.29 and Windows 13.0.2 live tests on new explicit target after separate approval; CLI privilege/version guards, migration interruption and least-privilege validation. Record exact outputs. |
| B7 — dev migration and acceptance, after B6 passes | Prepare concrete platform SQL/grants/final digest, private backup/recovery plan and exact-file requests; execute only when separately approved. No new features. | Owner runtime GUI/undo/restart, original-data preservation, expanded-schema backup/restore, final exact grants and explicit acceptance. Preserve all old/new evidence; no auto cleanup or Phase 1.3. |

File names above are proposed and may be consolidated within the same boundaries;
report material changes before implementation. Do not split a global chronological
undo stack per entity or introduce a generic UI framework to fit this list.
Migration/entity/audit port changes land coherently with regression tests rather
than leaving a task writer bypassing archive/lease validation.

## 10. Acceptance matrix and test requirements

| Area | Required evidence (not yet performed for Phase 1.2) |
| --- | --- |
| Ownership | Each valid parent row shape; both-set/both-NULL project/objective rejection; objective nesting/direct task-pillar rejection; task independent/objective/direct project; cross-dataset and missing/tombstoned owner rejection in commands and enforceable SQL FKs. |
| Depth/cycles | At least 1500-level iterative pillar/project fixtures without a product cap; navigation/path resolution; self, ancestor/descendant and corrupt graph cycles; two concurrent opposite moves serialize and one rejects. |
| Moves | Mixed pillar/project/objective subtrees; stable descendant/task UUIDs, fields, revisions and audit bytes; context/path refresh; stale preview rejected; archived-target warning; move/undo/redo preserves ownership rules. |
| Archive/restore | Parent archive hides descendant tasks in tree/flat views without changing flags; independently archived child remains archived after parent restore; own restore below archived ancestor stays effectively archived; move-out clears only inherited effect; completed/task lifecycle unaffected. |
| Dependency-safe removal | Undo-create rejects non-tombstoned children, archived children, completed dependent tasks and new external dependents; after child undo, parent undo/redo sequence safe; hard delete/subtree purge unavailable. |
| Commands/history | Create/edit/rename/move/archive/restore and task owner edits undoable; one mixed stack; atomic compound task editor; exact touched-field inverse, timestamp/revision monotonicity, failed compensation/no index drift, redo invalidation only on success, 100-limit and restart clear. |
| Atomic storage | Fail after entity write/receipt/outbox and during constraints: no partial domain/audit/change state. Unknown COMMIT acknowledgment clears history/requires reload. Same target in receipt/change is application-enforced and explicitly tested. |
| Upgrade/compatibility | Populated 001→002 preserves all legacy columns and JSON bytes; no row 1 rewrite/new dataset. Empty fresh install runs both in order; unknown/gap/checksum/schema drift/partial state refuses; interruption at each DDL/journal/metadata boundary; old runtime rejects schema 2/new runtime refuses schema 1 with instructions. |
| Desktop | Legal menus/editors/Move previews, parent-aware independent capture, subtree/objective views without duplicated rows, UUID selection/expansion through rename/move/filter, Show Archived/Completed independent, Qt model contract and keyboard accessibility. |
| Phase 1.1 regressions | All existing pure/GUI/offline safety contracts remain; explicit isolated task CRUD/lifecycle/history/restart/rollback scenarios also run on schema 2 without targeting retained integration evidence. No fixtures in production GUI. |
| Platforms/security | Linux and Windows native configure/build/CTest; real QMYSQL on MariaDB 10.5.29/13.0.2, exact minimal grants/PUBLIC, no new remote/global/runtime DDL privilege; runtime migration CLI rejected before connection; scripts mocked offline first. |
| Backup/restore | New populated schema-2 private dump, approved fresh-target restore, schema/dataset and all nine table counts/ordered exports match; old Phase 1.1 backup/exports/integration DBs preserved. |

Ordinary CTest must not require a live server, credentials, admin privileges or a
display (use existing offscreen/fake sessions). Live tests are a separate explicit
opt-in with skip 77 by default. Tests must not silently reset any pre-existing
schema; partial failures retain evidence. Linux success is not Windows evidence.

## 11. Decisions proposed for owner approval and risks

| ID | Choice requiring approval | Recommendation / practical consequence |
| --- | --- | --- |
| D1 | Archive/move policy recommendations | Adopt section 4: explicit/inherited flags; confirmed archived moves; block archived-owner creation; never restore descendants/ancestors silently. |
| D2 | Delete UI and entity completion lifecycles | No user-facing organizational Delete/purge or project/objective completion UI in 1.2. Implement dependency-safe tombstoning for undo-create, archive/restore only for user removal. If explicit leaf Delete/completion is required now, resolve its additional lifecycle/history semantics before B1/B2. |
| D3 | Duplicate names | Allow duplicates, ID/path disambiguation; no unstated case-insensitive unique names or automatic rename on restore. |
| D4 | Task-only receipts/stack extension | One typed four-entity command path and shared stack; nullable typed audit FKs + v2 payloads, preserving v1. Alternative separate org audit tables duplicate infrastructure; generic entityId loses FKs; neither recommended. |
| D5 | Concurrency | All mutations use schema lease + transaction-local rule/CAS checks, not target-only CAS or a new closure table. No extra runtime Dataset UPDATE grants. Raw SQL bypass remains outside guarantees. |
| D6 | Incremental runner / compatibility | Registry + local attempt journal, unchanged historical fingerprints, latest-state verify, new runtime requires schema 2 and old clients must close. No silent migration, downgrade, dual-version writing or partial resume. |
| D7 | Test target and views | New explicitly isolated phase12 test DB/account; leave old integration restores untouched. Project/objective views now, sprint view only with planning. Exact provisioning still needs separate live approval. |

Major risks: extending task-only history without losing failed-command semantics;
validating cross-entity read sets without reviving stale inverses; interprocess
cycles; implicit DDL commits/constraint gaps; fingerprint portability; table-case
and CHECK/FK ALTER differences; retained older test/validation helpers accidentally
used on upgraded schemas; hidden tasks under archive; deep Qt/model/path traversal.
The staged tests/maintenance windows address these, not speculative frameworks.
No unresolved naming-style exception is needed. Other future lifecycle, sync,
ordering and historical-path decisions remain deferred, not silently implemented.

## 12. Step A validation and actual changes — 2026-11-15

Created docs/milestones/Phase1.2.md and updated docs/Roadmap.md current status/link/
phase row for planning. No other files changed. Phase1.1.md is untouched, including
its final acceptance, previous failures/results, approvals and artifact hashes.
Migration 001 is untouched. No new SQL file, diagrams, source, scripts, database
connections/queries, migrations, backups/restores, accounts or grants were created
or executed. No installed dependencies or Git commit/merge/tag/push.

Executed read-only repository commands: git status --short, git branch --show-current;
rg/sed/cat source/design inspections; sha256sum migration 001 and Phase1.1.md;
local Markdown reference/fence/content consistency checks and git diff --check.
Validation results and exact final checks are recorded below after completion.
Application build/CTest were not rerun: this is documentation-only Step A, and
no Phase 1.2 executable implementation exists to validate. Phase 1.1 accepted
results remain historical evidence, not new tests. Proposed SQL is not claimed
syntax/live-tested by a SQL engine; that requires later isolated approval.

Primary references used to check proposed DDL/transaction assumptions:
[MariaDB ALTER TABLE syntax/privileges](https://mariadb.com/docs/server/reference/sql-statements/data-definition/alter/alter-table),
[constraints](https://mariadb.com/docs/server/reference/sql-statements/data-definition/constraint),
and [foreign keys](https://mariadb.com/docs/server/ha-and-performance/optimization-and-indexes/foreign-keys).
No recommendation here treats DDL as transactionally reversible.

## Approval Requested — Step B source implementation only

Approve the staged B1–B6 **source, migration-file, tests, diagrams and documentation**
work described here, including recommended decisions D1–D7, or specify changes.
B4/B6 live checks remain blocked until their own concrete approval; source-test
construction/offline runs can proceed. Live portion of B7 is not authorized by
source approval. Exact proposed SQL above is a review artifact, not execution consent.

Before any live provisioning, grants, integration DDL/DML/reset, dev migration or
backup/restore, present the final versioned SQL/files/checksums, exact target and
account, preflight, failure/recovery/rollback limits and command list for separate
owner approval, with full report here first. Preserve the two existing restored
integration databases and all original backups/exports. Stop on unapproved
architectural/scope changes. No Phase 1.3+, dependencies/installations or Git actions.

**Stopped at Step A. No Step B implementation or live database action has begun.**

## Step A final validation results — 2026-11-15

Passed: local Markdown links/fences; branch and two-file scope; static draft SQL
check (nine DDL statements, four CREATE TABLEs, existing five + four = nine tables,
known FK endpoints, no table/column drop/truncate/data deletion); accepted artifact
identity; and existing actual-schema diagram provenance. This is textual consistency
checking, not MariaDB syntax/privilege/migration execution. No Phase 1.2 tests,
Linux/Windows builds or live SQL validation have been run or claimed.

Reproducible read-only final commands:

```bash
git branch --show-current
git status --short
sha256sum docs/milestones/Phase1.1.md migrations/mariadb/001_PersistentTaskFoundation.sql
git diff --exit-code -- docs/milestones/Phase1.1.md migrations src tests scripts docs/diagrams
python3 docs/diagrams/GenerateDatabaseSchema.py --check
git diff --check
```

Results: branch jb_20261114_ms1p2; only Roadmap.md modified and Phase1.2.md new;
protected-file diff empty; diagram still describes actual migration 001 (five
tables/38 columns/four SQL FKs/two logical references). Hashes:

```text
ffbf123937642a3478662d817a4f522377bb0ae6271fec51810e5b735bc34bde  docs/milestones/Phase1.1.md
bb901d7580398893b5c04ee263f813b9d05873eb4bae08e0daf377faf73b0ea0  migrations/mariadb/001_PersistentTaskFoundation.sql
```

Markdown check used the following nonexecuting parser; no external packages:

```bash
python3 - <<'PY'
from pathlib import Path
import re
for name in ['docs/milestones/Phase1.2.md', 'docs/Roadmap.md']:
    path = Path(name)
    text = path.read_text()
    assert sum(line.startswith('```') for line in text.splitlines()) % 2 == 0
    for target in re.findall(r'\]\(([^)]+)\)', text):
        if '://' not in target and not target.startswith('#'):
            target = target.split('#')[0]
            if target:
                assert (path.parent / target).exists(), (name, target)
print('PASS: planning Markdown references and fences.')
PY
```

Final handoff: review D1–D7 and the Step B source-only approval request above.
No outstanding owner decision has been silently settled; all new physical choices
remain proposals. Phase 1.1 records/data/backup/restore evidence remain untouched.

## Step B authorization — 2026-11-16

Owner approved the general architecture/D1–D7 and source-only B1–B6 implementation.
Clarifications are binding: keep explicit/inherited archive distinct and archived
records easy to locate/restore; defer organization completion and user deletion;
one shared chronological history, touched-field inverses and concurrent-edit safety;
test mutation leases with separate connections including conflicts/timeouts/loss;
never resume/reset/repair partial migrations; validate 002 on separately approved
10.5/13 test targets before dev upgrades; iterative deep core/Qt models; preserve
all accepted Phase 1.1 data/history/artifacts/restored databases.

No live SQL/provisioning/grants/test resets/dev migration/backup/restore/destructive
operation is authorized. Such stages need separate exact target/SQL/recovery
approval. No commit/push/merge/tag. Initial step: B1 pure shared values/rules and
offline tests, then the shared command/history path. Subsequent reports append here.

## B1/B2 first source increment — 2026-11-16

Added standard-C++ Pillar/Project/Objective records, typed organization keys/parents,
TaskOwner, iterative OrganizationRules and optional task ownership command intent.
No lifecycle fields for project/objective completion or user-facing delete were
introduced. Pure tests cover legal/illegal parent shapes, dataset rejection,
self/ancestor cycles, inherited vs explicit archive, completed-task dependency
protection and a 2001-level hierarchy. The graph constructor memoizes iterative
ancestry validation; no recursive core traversal or depth cap.

Added one generic four-target CommandService/WorkspaceRepository transaction port,
receipt touched-field mask/read dependencies, bounded mixed chronological history,
commit-gated undo/redo and unknown-commit clearing. TaskService is being retained
as a thin compatibility facade for accepted schema-1 adapters/tests, delegating
history to the same CommandService (no independent history engine). Production
schema-2 workspace transaction/Widgets integration is still in progress.

Files so far: domain OrganizationFields/Pillar/Project/Objective/TaskOwner/
OrganizationRules, Task; application OrganizationCommand/WorkspaceRepository/
CommandService, TaskCommand/TaskService; test MemoryWorkspaceRepository,
OrganizationRulesTests/CommandServiceTests; CMake test/source registration.
Unit tests exercise mixed task/organization inverses, stable completion timestamps,
archive propagation without child writes, stale previews, rejected undo-create
with external completed tasks, rollback contract, redo preservation, external field
edits and uncertain commits. A first compile of the new fake repository rejected
conversion between two variant types; fixed by explicit type-aware revision access.
Initial missing-field warnings were corrected with a default owner initializer.

Commands executed: cmake configure/build with live DB tests OFF; ctest focused on
TaskCoreTests, OrganizationRulesTests and CommandServiceTests: 3/3 passed. Subsequent
TaskService facade compatibility build/test is in progress. No live database,
backup, account, migration execution or package installation occurred. Migration
001 and Phase1.1.md remain unchanged. B3–B6 and full regression validation remain.


## Source checkpoint and owner-requested pause — 2026-11-16

The owner requested a stopping point within five minutes before internet loss.
Feature work stopped; build, offline regression checks and this handoff were
completed. **Phase 1.2 is not complete or accepted.** No live SQL, database
connection, provisioning, grants, migration execution, test reset, backup/restore,
package installation or Git commit/push/merge/tag was performed.

### Implemented source, with validation limits

- B1/B2: typed organizational records and iterative ownership/cycle/archive rules;
  one shared chronological CommandService across task and organizational mutations.
  TaskService delegates to that engine for Phase 1.1 compatibility. Transaction
  callbacks prepare receipts from fresh state; inverses carry touched fields,
  revisions/dependencies and compensation links. Fake transactional tests cover
  rollback, stale state, failed undo/redo, redo retention and uncertain commits.
- B3: migration 002 and cumulative structural manifest, versioned incremental
  runner, partial-attempt journal, legacy-data preservation checks and explicit
  rejection of interrupted attempts. Migration 001 is unchanged. New migration
  source has **not** been executed. No automatic recovery/resume is provided.
- B4: prepared-query MariaDbWorkspaceRepository, typed version-2 payload codec,
  durable receipts/change rows in the entity transaction and database-scoped
  mutation lease. These paths compile; live correctness remains unverified.
- B5: persistent hierarchy model, project/objective projections, organization
  editor/move dialogs, parent-aware task capture/editing, archive visibility and
  ancestor-location/restore controls, stable navigation settings and shared undo.
  Iterative core/model tests exercise 2001-level hierarchies. Existing prototype
  remains; production startup now requires verified schema 2 and does not upgrade
  automatically. Do not use this new binary with the unchanged schema-1 development
  database until separately approved migration validation is complete.
- B6, partial: separate opt-in OrganizationIntegrationTests target for only
  `taskcompass_phase12_test` / `taskcompass_phase12_test@127.0.0.1:3306`, interruption
  fixtures, rollback hooks and separate-connection lease/conflicting-move tests.
  This executable compiled but has never connected to a database. Ordinary CTest
  skips both integration executables. Existing restored integration data remains
  untouched. A fixed-target provisioning helper extraction and Phase 1.2 SQL
  template/preview wrapper are prepared; operator automation is not finished or
  approved for execution. Legacy provisioning-script fake tests still pass.
- Schema diagrams and generator now reflect cumulative actual migration source:
  nine tables, 85 columns, 20 SQL foreign keys and two logical references. SVG was
  regenerated with installed Mermaid CLI and visually inspected using a local
  headless Firefox screenshot; no new dependency was installed. First sandboxed
  rendering failed; approved renderer execution succeeded. Diagram changes describe
  source schema, not a claim about any live database.

### Validation actually executed at this checkpoint

```bash
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
python3 docs/diagrams/GenerateDatabaseSchema.py --check
bash -n scripts/provision-integration.sh scripts/phase12/03-setup-test.sh temp/testdb/setup.sh
bash scripts/phase12/03-setup-test.sh --print-plan
git diff --check
sha256sum migrations/mariadb/001_PersistentTaskFoundation.sql docs/milestones/Phase1.1.md
```

Build succeeded (`ninja: no work to do` after earlier successful compilation).
CTest: **14 passed, two intentionally skipped, zero failed**, 5.89 seconds.
Skipped: MariaDbIntegrationTests and OrganizationIntegrationTests. This includes
legacy script, core, desktop and storage safety regressions, new rules/shared
commands/codec/desktop tests and schema documentation checks. Bash syntax and
placeholder-only preview succeeded without connection. Diagram consistency check
passed (9/85/20/2). Diff whitespace check passed before this report and is rerun
below. Initial development compile failures (variant conversion and missing
WorkspaceSnapshot include) were corrected before the successful final build/tests.
An intermediate test run used older binaries after a failed build; it is not
counted as validation of those source changes.

Protected files retain their baseline SHA-256:

- Migration 001: `bb901d7580398893b5c04ee263f813b9d05873eb4bae08e0daf377faf73b0ea0`.
- Phase1.1.md: `ffbf123937642a3478662d817a4f522377bb0ae6271fec51810e5b735bc34bde`.

### Known limitations and resume order

1. Finish B6 operator helpers, their fake safety tests and source documentation.
   Preview scripts are preparation only; do not run live provisioning or tests.
2. Review migration runner/manifest and CLI edge cases (including explicit target
   version zero); structural checks currently independently compare CHECK names,
   while exact expressions rely on approved SQL and schema fingerprints. Add
   stronger negative constraint/ownership coverage before live approval.
3. Review full-subtree move/archive impact reporting: current GUI move count is
   filtered-view based. Review duplicate-name disambiguation and optional notes
   empty/NULL fidelity. Live performance of per-operation schema verification is
   unmeasured. No material architectural deviation is approved by this checkpoint.
4. Finish integration test coverage review, especially invalid/cross-dataset SQL
   ownership, concurrent undo failures and failure recovery. Separate-connection
   timeout/loss/conflicting-move tests exist as source, not verified live results.
5. Update shared architecture/data-model/build documentation to distinguish newly
   implemented source from pending operational validation; rerun offline checks.
6. Only then present exact isolated-target SQL, grants, migration/recovery and
   backup procedures for separate approval. MariaDB 10.5 and 13 live validation,
   Windows compilation, interactive GUI review, restart persistence and expanded
   backup/restore are all pending. No development upgrade is authorized.

No further owner design decision was silently approved. The existing source-only
B1–B6 authorization remains the boundary when work resumes. No live approval is
requested at this pause checkpoint.

### Files created or modified at pause

The following inventory includes source-only work in this milestone; generated
Python bytecode, if present locally, is not a deliverable.

```text
 M CMakeLists.txt
 M docs/Roadmap.md
 M docs/diagrams/DatabaseSchema.md
 M docs/diagrams/DatabaseSchema.svg
 M docs/diagrams/GenerateDatabaseSchema.py
 M docs/milestones/Phase1.2.md
 M src/application/TaskCommand.h
 M src/application/TaskService.cpp
 M src/application/TaskService.h
 M src/desktop/persistent/PersistentMain.cpp
 M src/desktop/persistent/PersistentTaskController.cpp
 M src/desktop/persistent/PersistentTaskController.h
 M src/desktop/persistent/PersistentTaskFilter.cpp
 M src/desktop/persistent/PersistentTaskFilter.h
 M src/desktop/persistent/PersistentTaskModel.cpp
 M src/desktop/persistent/PersistentTaskModel.h
 M src/desktop/persistent/PersistentTaskSession.h
 M src/desktop/persistent/PersistentTaskWindow.cpp
 M src/desktop/persistent/PersistentTaskWindow.h
 M src/domain/Task.cpp
 M src/domain/Task.h
 M src/infrastructure/storage/DatabaseConfig.cpp
 M src/infrastructure/storage/DatabaseConfig.h
 M src/infrastructure/storage/MigrateMain.cpp
 M src/infrastructure/storage/MigrationRunner.cpp
 M src/infrastructure/storage/MigrationRunner.h
 M src/infrastructure/storage/Migrations.qrc
 M temp/testdb/setup.sh
?? docs/diagrams/__pycache__/
?? migrations/mariadb/002_OrganizationalManagement.sql
?? migrations/mariadb/SchemaManifest.json
?? scripts/phase12/
?? scripts/provision-integration.sh
?? src/application/CommandService.cpp
?? src/application/CommandService.h
?? src/application/OrganizationCommand.h
?? src/application/WorkspaceRepository.h
?? src/desktop/dialogs/OrganizationEditDialog.cpp
?? src/desktop/dialogs/OrganizationEditDialog.h
?? src/desktop/dialogs/OrganizationMoveDialog.cpp
?? src/desktop/dialogs/OrganizationMoveDialog.h
?? src/desktop/models/OrganizationTreeModel.cpp
?? src/desktop/models/OrganizationTreeModel.h
?? src/domain/Objective.h
?? src/domain/OrganizationFields.h
?? src/domain/OrganizationRules.cpp
?? src/domain/OrganizationRules.h
?? src/domain/Pillar.h
?? src/domain/Project.h
?? src/domain/TaskOwner.h
?? src/infrastructure/storage/MariaDbWorkspaceRepository.cpp
?? src/infrastructure/storage/MariaDbWorkspaceRepository.h
?? src/infrastructure/storage/MutationLease.cpp
?? src/infrastructure/storage/MutationLease.h
?? src/infrastructure/storage/WorkspaceCodec.cpp
?? src/infrastructure/storage/WorkspaceCodec.h
?? tests/CommandServiceTests.cpp
?? tests/DatabaseSchemaTests.py
?? tests/MemoryWorkspaceRepository.h
?? tests/OrganizationDesktopTests.cpp
?? tests/OrganizationIntegrationTests.cpp
?? tests/OrganizationRulesTests.cpp
?? tests/OrganizationTestSafety.h
?? tests/WorkspaceCodecTests.cpp
```


## Step B resumed: source review handoff — 2026-11-16

Owner requested continuation after the pause. Source-only B1–B6 authorization
remains in effect; no live authorization was inferred. B1–B5 source and B6 tooling,
diagrams and offline checks are delivered for review. B6 live/platform validation
and B7 development upgrade/acceptance are **pending**, not complete. No production
or test database connection/SQL, account/grant change, reset, backup/restore,
dependency installation or Git commit/push/merge/tag occurred in this continuation.

### Changes and decisions since the pause

- Finished numbered Phase 1.2 helpers: 01-build, 02-review-plan, 03-setup-test,
  04-integration-test. Help/preview never connects; setup needs --apply --approved
  plus exact typed confirmation. Test execution has its own --approved/confirmation,
  fresh-empty catalog proof and read-only administrator grant/role/PUBLIC preflight.
  Test credentials are separately scoped; existing conflicting environment values
  fail before connecting. Unrelated PUBLIC test/test_% grants are accepted without
  allowing matching write/DDL grants. No TLS verification setting is changed.
- Extracted the established fresh provisioner to scripts/provision-integration.sh
  with exactly two fixed presets. temp/testdb/setup.sh retains its Phase 1.1 CLI
  as a compatibility wrapper; legacy mock tests pass. The new preset never selects
  taskcompass_dev or taskcompass_integration. No general arbitrary-target option
  was introduced. Phase 1.1's build helper now explicitly disables **both** live
  CTest options, preventing a cached Phase 1.2 opt-in from making that build live.
- GUI move/archive/restore previews now count the entire subtree, including hidden,
  completed and archived work, rather than the filtered table. Confirmation is
  bound to workspace revisions and revalidated transactionally. Duplicate names
  remain legal; navigation/pickers show kind/UUID suffixes when names repeat and
  full paths/tooltips. Organization notes preserve unchanged NULL versus empty text.
- Strengthened tests for typed parent combinations, 2001-level pillar and 2002-level
  project paths, mixed entity history, subtree moves with untouched descendant
  revisions/flags, explicit versus inherited archive, failed redo and restart.
  Desktop fake sessions cover archive discovery/ancestor restoration and duplicate
  names/notes. No real persistent GUI review has been claimed.
- Added migration CLI guards rejecting explicit target 0/unknown targets and
  ambiguous target-without-action calls before connection. New process tests cover
  these guards, runtime --apply rejection and new-suite opt-in/configuration errors
  without a connection. Test cleanup policy is independently exercised offline.
- Fresh migration initialization now rejects visible unexpected stored code before
  DDL, complementing the required administrator preflight. Incomplete attempts still
  require review; no resume/repair/reset path was added.
- Live-suite source now includes SQL constraint/ownership probes, cross-dataset FK
  rejection, typed receipt/change target checks and migration-history tamper cases.
  Probes roll back their synthetic DML fixtures. Expected fault-injection cleanup
  requires proof the requested injection was reached; an earlier unexpected error
  stops and preserves partial state instead of being mistaken for an expected fault.
  Lease-busy tests additionally verify the second connection is alive, distinguishing
  a genuine busy result from loss of that connection. None of this has run live.
- Updated README, Requirements, Architecture, DataModel, Interactions, Roadmap and
  Building to distinguish accepted Phase 1.1, new schema-2 source and pending live
  validation. Added [TestingPhase1.2.md](../TestingPhase1.2.md) with numbered commands.
  Standard C++ domain/application, Qt Widgets, existing task IDs/lifecycle and
  future-feature boundaries are unchanged. No D1–D7 architectural change is proposed.

The source schema diagram remains cumulative 001+002: **9 tables, 85 columns,
20 enforced SQL FKs and 2 logical references**. The generated structural registry
is tested against both cumulative source versions. Only generated Python bytecode
is newly ignored; it is not a deliverable. Earlier diagram rendering/visual evidence
remains in the checkpoint above; unchanged SVG did not need another render here.

### Agent-executed validation and observed failures

Host tools: GCC **11.5.0**, CMake **3.31.8**, Qt **6.10.3**. No current MariaDB server
version was queried; 10.5.29/Linux and 13.0.2/Windows remain the intended owner
validation targets, not newly verified observations.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
   -DTASKCOMPASS_ENABLE_DB_TESTS=OFF -DTASKCOMPASS_ENABLE_PHASE12_DB_TESTS=OFF
cmake --build build --parallel 2
bash tests/Phase12ValidationScripts.test.sh
scripts/phase12/01-build.sh
python3 docs/diagrams/GenerateDatabaseSchema.py --check
bash -n scripts/provision-integration.sh scripts/phase12/*.sh temp/testdb/setup.sh
scripts/phase12/02-review-plan.sh
scripts/phase12/03-setup-test.sh --print-plan
scripts/phase12/04-integration-test.sh --print-plan
git diff --check
sha256sum migrations/mariadb/001_PersistentTaskFoundation.sql \
   migrations/mariadb/002_OrganizationalManagement.sql docs/milestones/Phase1.1.md
```

Final numbered build/configure succeeded. Its full CTest run: **15 passed,
2 deliberately skipped, 0 failed**, 6.28 seconds. Skips: MariaDbIntegrationTests
and OrganizationIntegrationTests. There are 17 registered tests, not 17 live passes.
The two integration executables compile but ordinary execution returns 77 without
configuration or SQL. Shell safety tests use fake clients/executables only. The
original prototype startup runs offscreen; real GUI/manual and Windows validation
are pending. Source-only previews and shell syntax passed; diagram check passed.
Documentation reference/fence consistency and final whitespace checks are recorded
below after this report is written.

Intermediate failures in this continuation: the new manifest comparison initially
compared JSON lists with parser tuples rather than normalizing through JSON, and
an added deep-project fixture duplicated UUIDs already covered by an existing
2002-level project fixture. Corrected the comparison and removed the duplicate
fixture. Both tests pass in the final run; neither failure justified a production
schema or ownership change. The earlier 14-pass checkpoint remains historical.

### Remaining limits and review requirements

- No live SQL compatibility, isolation/constraint, transaction, locking or migration
  recovery result yet for 002 on either server. Source tests do not establish these.
- CHECK names, FK targets, columns/types/nullability and explicit indexes are checked
  structurally. CHECK expression semantics are not independently parsed/compared;
  source-checksum verification, registered DDL, conservative latest fingerprint and
  live negative constraint tests are complementary checks. Fingerprints can change
  with server formatting; stop for review, never rewrite metadata automatically.
- Named leases serialize cooperating schema-2 clients only. Old running clients
  and raw SQL bypass that lease, so all writers must stop for upgrades/tests.
  Restricted metadata cannot prove absence of hidden stored code; administrator
  preflight is mandatory before tests. Unexpected failure preserves evidence.
- Per-operation schema verification/coherent graph loads favor correctness;
  live latency and large-workspace performance remain unmeasured. Ancestor traversal
  and our model ownership are iterative; there is no product depth cap.
- Preview revision sets are conservative: any workspace change invalidates a stale
  impact/owner confirmation. External revision conflicts reject undo; there is no
  automatic merge of an external edit. A rejected command leaves history intact;
  an uncertain commit clears it and requires reload/inspection.
- Session history remains bounded/nonpersistent. Durable receipts are retained but
  are not a cross-restart undo UI, sync implementation or durable-history recovery.
- Windows build/CTest, both MariaDB live suites, interactive keyboard/navigation,
  mixed undo/redo, archive/owner/restart persistence and expanded-schema private
  backup/restore still gate milestone completion. No Phase 1.3 behavior was added.

MariaDB privilege rationale was cross-checked against primary documentation:
[ALTER TABLE privileges](https://mariadb.com/docs/server/reference/sql-statements/data-definition/alter/alter-table)
and [GRANT/index privileges](https://mariadb.com/docs/server/reference/sql-statements/account-management-sql-statements/grant).
ALTER suffices for indexes introduced by ALTER; standalone INDEX is not requested.
Actual least-privilege behavior still requires both approved server validations.

### Approval Requested — Stage 1A: fresh test provisioning only

Request **separate explicit approval for each intended host**: Linux/CentOS MariaDB
10.5.29 and later Windows/UCRT64 MariaDB 13.0.2, local TCP 127.0.0.1:3306.
Target database and account: **taskcompass_phase12_test**, account host **127.0.0.1**.
No existing database/account is to be changed or reused. Existing development,
diagnostic/runtime/migration accounts and restored Phase 1.1 databases/artifacts
are outside this request. If either name already exists, stop and report it.

Exact provisioning template (password placeholder is substituted only in memory):

```sql
SET SESSION sql_mode='NO_BACKSLASH_ESCAPES';
CREATE DATABASE taskcompass_phase12_test
   CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER 'taskcompass_phase12_test'@'127.0.0.1'
   IDENTIFIED BY '<distinct-20+-character-password-from-password-manager>';
GRANT SELECT, INSERT, UPDATE, CREATE, DROP, ALTER
   ON `taskcompass\_phase12\_test`.*
   TO 'taskcompass_phase12_test'@'127.0.0.1';
```

Template: scripts/phase12/test-account.sql, SHA-256
`2ed6b69647dabeba4f2eb8f2da815cd54d5b4c35903a439aafe44c861be40a1c`.
Read-only preflight before provisioning:

```sql
SELECT @@hostname,VERSION(),@@port;
SELECT @@port,
   (SELECT COUNT(*) FROM information_schema.SCHEMATA
      WHERE SCHEMA_NAME='taskcompass_phase12_test'),
   (SELECT COUNT(*) FROM mysql.user WHERE User='taskcompass_phase12_test');
```

Require approved hostname/version/platform, port 3306 and counts zero/zero. The
script reports identity and accepts only MariaDB 10.5/13 series; any unexpected
patch/host still needs owner review. After approval the operator command is:

```bash
scripts/phase12/03-setup-test.sh --apply --approved
```

No migration or integration execution is included in Stage 1A approval. Risk:
CREATE DATABASE/USER/GRANT is not one atomic transaction; failure may leave a fresh
partial provision. Stop and retain output, inspect actual resources/privileges
read-only, then obtain a concrete recovery decision. No automatic ALTER USER,
GRANT repair, DROP USER/DATABASE, retry or cleanup is authorized. No prior data
needs overwriting/backing up because only a nonexistent target is accepted.

### Stage 1B preview — not included in Stage 1A approval

After reviewing Stage 1A's actual result, request separate authorization for
`scripts/phase12/04-integration-test.sh --approved` on the same specific host/target.
Its initial administrator queries are read-only, scoped to target catalog and test
account grants/roles plus applicable PUBLIC policy. It requires the exact six
schema privileges, no global privileges beyond USAGE, no table/column extras or
explicit roles, no unexpected inherited authority, and an empty utf8mb4 target.
It then delegates using only test-account credentials/environment.

The source migration SQL is exactly section 6's nine statements, now in the actual
002 file (not merely a proposal), plus unchanged 001 for synthetic baseline setup:

- 001 SHA-256: `bb901d7580398893b5c04ee263f813b9d05873eb4bae08e0daf377faf73b0ea0`.
- 002 SHA-256: `7de311935843e4fc04cbca74d025d9f38986a4154d70bdd29b35a2820f87b736`.

The suite creates synthetic tasks/history, upgrades them, injects twelve deliberate
migration interruptions, validates rejection of partial attempts, probes constraints
inside rolled-back DML transactions and tests serial mutation leases/commands.
Expected fault fixtures are explicitly run-owned disposable resets under that
future approval. Unexpected errors do not trigger cleanup. At successful fixture
boundaries only the following statements can remove run-owned objects:

```sql
DROP TABLE IF EXISTS taskcompass_phase12_test.LocalChange;
DROP TABLE IF EXISTS taskcompass_phase12_test.CommandRecord;
DROP TABLE IF EXISTS taskcompass_phase12_test.Task;
DROP TABLE IF EXISTS taskcompass_phase12_test.Objective;
DROP TABLE IF EXISTS taskcompass_phase12_test.Project;
DROP TABLE IF EXISTS taskcompass_phase12_test.Pillar;
DROP TABLE IF EXISTS taskcompass_phase12_test.MigrationAttempt;
DROP TABLE IF EXISTS taskcompass_phase12_test.Dataset;
DROP TABLE IF EXISTS taskcompass_phase12_test.SchemaMigration;
```

These do not adopt/reset pre-existing objects. Interrupted/failed attempts outside
intentional injected fixtures need explicit review: preserve catalog, attempt row,
migration history and output; do not rerun/reset/repair. No generic down migration
or transactional DDL rollback is promised. Request recovery SQL separately after
inspection. Successful suite leaves **this new test schema** empty; it never touches
taskcompass_integration. Development migration/privileges, real-data rehearsal,
backup/restore and GUI use remain separate later approvals after both server tests.

### Current changed-file inventory for independent review

```text
 M .gitignore
 M CMakeLists.txt
 M README.md
 M docs/Architecture.md
 M docs/Building.md
 M docs/DataModel.md
 M docs/Interactions.md
 M docs/Requirements.md
 M docs/Roadmap.md
 M docs/diagrams/DatabaseSchema.md
 M docs/diagrams/DatabaseSchema.svg
 M docs/diagrams/GenerateDatabaseSchema.py
 M docs/milestones/Phase1.2.md
 M scripts/phase11-validate.sh
 M src/application/TaskCommand.h
 M src/application/TaskService.cpp
 M src/application/TaskService.h
 M src/desktop/persistent/PersistentMain.cpp
 M src/desktop/persistent/PersistentTaskController.cpp
 M src/desktop/persistent/PersistentTaskController.h
 M src/desktop/persistent/PersistentTaskFilter.cpp
 M src/desktop/persistent/PersistentTaskFilter.h
 M src/desktop/persistent/PersistentTaskModel.cpp
 M src/desktop/persistent/PersistentTaskModel.h
 M src/desktop/persistent/PersistentTaskSession.h
 M src/desktop/persistent/PersistentTaskWindow.cpp
 M src/desktop/persistent/PersistentTaskWindow.h
 M src/domain/Task.cpp
 M src/domain/Task.h
 M src/infrastructure/storage/DatabaseConfig.cpp
 M src/infrastructure/storage/DatabaseConfig.h
 M src/infrastructure/storage/MigrateMain.cpp
 M src/infrastructure/storage/MigrationRunner.cpp
 M src/infrastructure/storage/MigrationRunner.h
 M src/infrastructure/storage/Migrations.qrc
 M temp/testdb/setup.sh
?? docs/TestingPhase1.2.md
?? migrations/mariadb/002_OrganizationalManagement.sql
?? migrations/mariadb/SchemaManifest.json
?? scripts/phase12/
?? scripts/provision-integration.sh
?? src/application/CommandService.cpp
?? src/application/CommandService.h
?? src/application/OrganizationCommand.h
?? src/application/WorkspaceRepository.h
?? src/desktop/dialogs/OrganizationEditDialog.cpp
?? src/desktop/dialogs/OrganizationEditDialog.h
?? src/desktop/dialogs/OrganizationMoveDialog.cpp
?? src/desktop/dialogs/OrganizationMoveDialog.h
?? src/desktop/models/OrganizationTreeModel.cpp
?? src/desktop/models/OrganizationTreeModel.h
?? src/domain/Objective.h
?? src/domain/OrganizationFields.h
?? src/domain/OrganizationRules.cpp
?? src/domain/OrganizationRules.h
?? src/domain/Pillar.h
?? src/domain/Project.h
?? src/domain/TaskOwner.h
?? src/infrastructure/storage/MariaDbWorkspaceRepository.cpp
?? src/infrastructure/storage/MariaDbWorkspaceRepository.h
?? src/infrastructure/storage/MutationLease.cpp
?? src/infrastructure/storage/MutationLease.h
?? src/infrastructure/storage/WorkspaceCodec.cpp
?? src/infrastructure/storage/WorkspaceCodec.h
?? tests/CommandServiceTests.cpp
?? tests/DatabaseSchemaTests.py
?? tests/MemoryWorkspaceRepository.h
?? tests/OrganizationDesktopTests.cpp
?? tests/OrganizationIntegrationTests.cpp
?? tests/OrganizationRulesTests.cpp
?? tests/OrganizationTestSafety.h
?? tests/Phase12ValidationScripts.test.sh
?? tests/WorkspaceCodecTests.cpp
```


### Final documentation consistency checks — 2026-11-16

Agent-executed local-reference/fenced-block checks passed for README, Requirements,
Architecture, DataModel, Interactions, Roadmap, Building, TestingPhase1.2,
Phase1.2 and DatabaseSchema Markdown (10 files). Section 6's SQL block equals the
actual 002 file exactly after trimming terminal whitespace. All three numbered
preview routes and Bash syntax validation passed without a database connection;
the previews were redirected to /tmp/taskcompass-phase12-* for inspection and
contain only source/placeholder SQL, no credentials or application data.
Diagram consistency passed (9/85/20/2). git diff --check passed and is rerun after
this final entry. Migration 001 and Phase1.1.md hashes still match the protected
baseline above. Existing backup/export/restored databases were not accessed.

The recorded connection-loss fixture uses controlled connection closure while a
lease is held; it is not a verified network-outage or uncertain-commit experiment.
All live fixtures remain unexecuted. Intermediate failed-test diagnostics included
`Duplicate or cross-dataset organization record.` and a JSON structure assertion;
these were test-fixture errors corrected before the final passing run.

**Next action:** owner review of source changes and Stage 1A approval for the first
specific host. Stop here. Stage 1B testing, development migration, privileges,
backup/restore and milestone acceptance remain separate pending checkpoints.
