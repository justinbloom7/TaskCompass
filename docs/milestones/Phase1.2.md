<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Phase 1.2 — Organizational Management

**Status: Step A planning completed for owner review on 2026-11-15. Step B is not
approved or started.** Branch inspected: `jb_20261114_ms1p2`; working tree was clean.
Only this document and the roadmap are changed by Step A. All designs/SQL below
are proposals, not implemented behavior or permission to execute SQL.

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
