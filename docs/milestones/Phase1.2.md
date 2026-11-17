<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Phase 1.2 — Organizational Management

**Status: isolated migration/integration validation passed on Linux/MariaDB 10.5.29 and Windows/MariaDB 13.0.2 (owner-reported). Development upgrade, narrow runtime/migrator privilege validation, real GUI/restart and backup/restore gates remain pending. Phase 1.2 is not accepted.**
Step A completed on 2026-11-15. Branch inspected: `jb_20261114_ms1p2`; working tree was clean.
Only this document and the roadmap were changed by Step A. Its designs/SQL were
proposals. Step A itself authorized no SQL; the dated Step B reports below record
subsequent source implementation, narrowly scoped live approvals and actual results.

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


## Stage 1A administrator socket authentication — 2026-11-16

Owner reports CentOS MariaDB root normally authenticates through the Unix socket.
The default helper selected TCP and requested a MariaDB password. Existing
`--admin-socket` already selected the socket and omitted that prompt, but did not
change the OS identity; a normal Linux user may therefore be unable to authenticate
as MariaDB root. This is a transport/OS-authentication issue, not evidence of a lost
password or a reason to change MariaDB authentication/accounts.

### Source changes and preserved boundaries

- `scripts/provision-integration.sh`: new explicit Linux `--admin-sudo` implies
  socket mode. After the existing approval/typed-confirmation gates, validate sudo
  authorization (`sudo -v`) before SQL or new test credentials are prepared. Each
  database invocation uses `sudo -n -- <client> --no-defaults ... --protocol=SOCKET
  --user=root`; only the client is elevated, not the repository script. No `sudo -S`,
  stored OS password, TCP fallback, server-authentication changes or repair logic.
- Both direct/socket-sudo modes remove inherited MYSQL_PWD before the client;
  TCP/password operation remains unchanged. Sudo denial/expiry fails closed.
  Windows rejects socket/elevation flags and retains its TCP password workflow.
- Corrected Phase 1.2 help to show the wrapper's required --approved argument.
  SQL templates, proposed privileges, fixed target/port checks, fresh-resource
  inspection and existing-resource rejection remain unchanged. No new packages.
- `tests/Phase12ValidationScripts.test.sh`: fake sudo/client coverage proves no
  MariaDB password input is consumed in socket mode, no MYSQL_PWD reaches sudo or
  the socket client, only the fixed client is elevated, and denial/expiry/Windows
  rejection cannot trigger SQL. Existing TCP and Phase 1.1 fake suites remain.
- `docs/TestingPhase1.2.md`: documented command, Linux-versus-DB-versus-test password
  distinctions and the current Stage 1B limitation. This milestone records the
  review and validation; prior proposals/results remain historical.

After separate Stage 1A approval, the intended CentOS operator command is:

```bash
bash scripts/phase12/03-setup-test.sh --apply --approved --admin-sudo
```

The sudo prompt, if present, is for the Linux login password. Only the new
`taskcompass_phase12_test` password is then requested twice by this script. For an
OS account already authorized for direct socket login, use --admin-socket instead.
Do not elevate the whole script. The provisioned account remains
`taskcompass_phase12_test@127.0.0.1` for TCP; the administrator's socket login is
separate. Setup's SQL/partial-failure recovery and Stage 1A approval scope above
remain unchanged. **No provisioning approval or execution is inferred from this
source-only request.**

### Validation

Executed only fake clients/sudo, syntax checks, no-connection help/preview and
local automated tests. Commands/results are recorded below after final checks.
No actual sudo, client connection, SQL, database/account/grant/authentication change,
backup/restore, cleanup or Git commit/push/merge/tag was executed. C++ application
source, migration SQL and schema diagrams are unaffected; rebuilding C++ is not
needed for this shell/documentation-only adjustment.

### Remaining work and Approval Requested

Review this Stage 1A authentication correction alongside the existing concrete
Stage 1A SQL/procedure request. Live provisioning still needs explicit approval
for the specific host/target. Real CentOS socket/sudo execution has not been tested.
Stage 1B remains a separate approval and currently still requests TCP administrator
authentication: adapt its read-only preflight for a socket-only administrator before
that later execution, rather than changing root authentication. No Stage 1B live
SQL, test resets, development upgrade or milestone acceptance is requested here.

### Socket-authentication validation results

```bash
bash -n scripts/provision-integration.sh tests/Phase12ValidationScripts.test.sh scripts/phase12/03-setup-test.sh
bash tests/Phase12ValidationScripts.test.sh
ctest --test-dir build --output-on-failure
bash scripts/phase12/03-setup-test.sh --help
bash scripts/phase12/03-setup-test.sh --print-plan
git diff --check
```

Results: syntax, expanded mock suite, no-connection help/preview and whitespace
checks passed. Full CTest: **15 passed, 2 intentionally skipped, 0 failed**, 6.32
seconds. Both live integration executables skipped. Python pathlib/re checks of
both changed Markdown files passed local-link existence and code-fence balance.
No C++ rebuild was needed because source, schema and CMake were unchanged.

During test editing, a duplicated socket mock scenario caused an accumulated
sudo-call-count assertion to fail. Removed the duplicate fixture block; the final
expanded suite and full CTest passed. The Linux-only branch uses a mock uname so
it can also be exercised by future UCRT64 CTest runs; actual Windows execution
was not performed here. No real sudo or database client was executed. The only
files changed are the shared provisioning helper, its Phase 1.2 mock test, the
Phase 1.2 testing guide and this milestone report.


## Stage 1A first connection failure — 2026-11-16

Owner attempted the socket/sudo provisioning command and reported:

```text
ERROR 2002 (HY000): Can't connect to local MySQL server through socket '/var/lib/mysql/mysql.sock' (2)
Administrator command failed. Check service/authentication; if provisioning began, preserve evidence and inspect before retrying.
```

The error occurred at the first server-identity connection, before the fresh-name
inspection, new-account password prompts or provisioning SQL. **This attempt
created no database/account and changed no privileges.** This conclusion concerns
this reported attempt only, not an assertion that the target is absent from every
possible prior operation.

Agent read-only investigation: local server configuration specifies
`socket=/var/lib/mysql/mysql.sock`; no socket was present at that path. The first
sandboxed systemctl read could not access the system bus (`Operation not permitted`).
After tool approval, executed outside the sandbox:

```bash
systemctl is-active mariadb
```

Result: **inactive**, exit code **3**. No SQL/client connection, sudo invocation,
service start/restart, database provisioning, repair or cleanup was executed by the
agent. Existing databases/artifacts and authentication remain untouched.

Recommended owner action: `sudo systemctl start mariadb`, then
`systemctl is-active mariadb` (expect active), then retry the approved Stage 1A
command. If startup fails, inspect service status and stop; do not initialize a
new database directory. If active but the socket remains absent, investigate socket
configuration before retrying. No source fix or authentication change is indicated
by the observed inactive service. This does not authorize agent provisioning or
Stage 1B testing.

Files modified: docs/TestingPhase1.2.md (service/socket troubleshooting) and this
milestone report. Application, scripts, SQL, schema diagrams and Phase 1.1 historical
reports are unchanged. Documentation-only validation: local Markdown references,
fence balance and git diff --check; results recorded below. No build/CTest rerun is
needed because executable source and test inputs did not change.

Validation result: modified Markdown references/fences passed; git diff --check passed.

## Owner-approved test password minimum — 2026-11-16

Owner explicitly requested lowering the fresh test-account password minimum to
12 characters. The prior 20-character threshold was a helper implementation
policy, not a MariaDB requirement. This decision supersedes earlier 20+ character
instructions/placeholders in the historical reports above.

Changed scripts/provision-integration.sh to use MIN_PASSWORD_LENGTH=12, with
matching prompt/error text. Because this is the shared fresh integration-account
helper, both fixed Phase 1.1 and Phase 1.2 presets use 12. No root/sudo password
policy, existing stored password, account, grant, target, approval or preflight rule
changed. Updated docs/TestingPhase1.2.md and added mock boundary tests to
tests/Phase12ValidationScripts.test.sh: exactly 12 succeeds; 11 stops before any
provisioning SQL reaches the mock. Existing-resource refusal remains unchanged.

Validation executed:

```bash
bash -n scripts/provision-integration.sh tests/Phase12ValidationScripts.test.sh
ctest --test-dir build -R '^(Phase12ValidationScripts|IntegrationDatabaseScripts|Phase11ValidationScripts)$' --output-on-failure
git diff --check
```

Syntax passed; all three offline mock suites passed (1.54 seconds). Markdown
reference/fence checks and final whitespace check passed. No live client/sudo/SQL,
provisioning, password reset or database changes were executed. C++/migration files
are unaffected, so compilation/full application testing was not repeated. No commit
or push. The prior service troubleshooting edits remain preserved in the working
tree; the earlier Stage 1A live approval/review checkpoints still apply.


## Linux Stage 1A owner-reported success — 2026-11-16

Owner supplied successful output from:

```bash
scripts/phase12/03-setup-test.sh --apply --approved --admin-sudo
```

Reported server: **feledria / 10.5.29-MariaDB / port 3306**, platform **Linux /
Linux**. The exact target confirmation was entered. The helper requested a new
12+ character integration password twice and reported:

```text
Created the isolated integration database/account with six scoped privileges.
No migrations/tests ran. Review grants and inherited policy before running tests.
```

Record Stage 1A Linux provisioning as **owner-reported successful**, not an
agent-executed operation. Target/account are taskcompass_phase12_test and
taskcompass_phase12_test@127.0.0.1 respectively. No migrations or integration
execution are reported. Exact grants, inherited authority and empty catalog still
require the independent Stage 1B preflight; this transcript is not that validation.
Windows Stage 1A remains pending. Existing Phase 1.1 databases/backups/restores are
outside this operation and remain preserved. Do not rerun setup on this existing
resource expecting repair; it is intentionally refused.

### Stage 1B socket administrator preflight prepared (source only)

The earlier Stage 1B TCP-only limitation is now superseded. Within existing
source-only authorization, extended scripts/phase12/04-integration-test.sh with
--admin-socket and --admin-sudo for the read-only administrator preflight. It follows
the reviewed Stage 1A pattern: validate sudo only after explicit approval and typed
confirmation, elevate only the MariaDB client, use --protocol=SOCKET, remove
MYSQL_PWD, use sudo -n to fail closed, no password capture or fallback. Windows
continues using TCP/password and rejects socket options.

The integration executable is **never elevated** and retains its own fixed TCP
configuration/test credentials. Empty catalog, exact six direct schema grants,
no unexpected global/table/column grants/roles/PUBLIC authority, fixed host/port,
identity, explicit flags and run-owned cleanup safeguards are unchanged. No
provisioning/account/grant/schema SQL was added or executed by the agent.

tests/Phase12ValidationScripts.test.sh now exercises both socket administrator
modes with fake sudo/client, verifies the test binary still uses the ordinary TCP
account, and blocks test delegation on sudo denial/expiry, nonempty schema, extra
grants, unsafe PUBLIC rights and Windows socket invocation. No real sudo or
client was run. docs/TestingPhase1.2.md now documents the runnable Linux route.

### Approval Requested — Stage 1B Linux isolated integration tests

Stage 1A's success does not authorize Stage 1B. Request approval for **only**:

- Host feledria, MariaDB 10.5.29, local TCP **127.0.0.1:3306** for the test account;
  administrator read-only checks use the local Unix socket via sudo.
- Database/account **taskcompass_phase12_test**, account host **127.0.0.1**.
- Administrator read-only preflight, then the synthetic migration/transaction/
  ownership/history/lease suite described in the Stage 1B preview above.
- Actual migration 001 and 002 SQL and hashes are unchanged from that preview.
  Its nine fully qualified DROP statements apply only to run-owned fixtures after
  empty-start proof. Twelve intentional interruption fixtures may be cleaned up
  only when the exact injected failure was reached; unexpected failure preserves
  evidence and requires separate review/recovery. No pre-existing data is adopted.

After approval, operator command:

```bash
scripts/phase12/04-integration-test.sh --approved --admin-sudo
```

Type `TEST taskcompass_phase12_test WITH RUN-OWNED RESET`. Supply the Linux/sudo
password if asked, then the **test-account password created in Stage 1A**. No MariaDB
root password is requested. Successful suite exit is 0 and the new schema is left
empty; failure stops without automatic repair/retry/reset. Preserve output and
partial state on failure; use reviewed recovery SQL only after further approval.
This request includes no development migration, change to any existing account/
privilege, retained taskcompass_integration database, backup/restore or Windows
execution. Both MariaDB platforms must still pass before development upgrade.

### Offline validation

Commands/results follow after final execution. No live database queries or tests
were executed by the agent. C++/migration code is unchanged, so no rebuild is
required for this shell/documentation-only continuation.

Executed validation commands:

```bash
bash -n scripts/phase12/04-integration-test.sh tests/Phase12ValidationScripts.test.sh
ctest --test-dir build -R '^(Phase12ValidationScripts|IntegrationDatabaseScripts|Phase11ValidationScripts)$' --output-on-failure
scripts/phase12/04-integration-test.sh --help
scripts/phase12/04-integration-test.sh --print-plan
sha256sum migrations/mariadb/001_PersistentTaskFoundation.sql migrations/mariadb/002_OrganizationalManagement.sql
git diff --check
```

Results: all three offline mock suites passed (1.73 seconds); syntax, no-connection
help/preview, Markdown link/fence consistency and whitespace checks passed. Both
live CMake options remain OFF. Migration hashes equal the approved-source preview
values above. No real sudo/client, live preflight, integration tests or database
changes were performed by the agent. Stop for Stage 1B Linux approval.

## Stage 1B Linux approval recorded — 2026-11-16

Owner explicitly approved Stage 1B in response to the concrete request above.
Authorization covers feledria / MariaDB 10.5.29, the fresh isolated
taskcompass_phase12_test database/account, read-only socket/sudo administrator
preflight, migrations 001/002 on synthetic fixtures and the documented run-owned
cleanup/negative tests. It does not authorize Windows execution, development
migration, account/grant changes, backup/restore, cleanup of unexpected failure
state, or access/reset of retained taskcompass_integration data.

Owner execution command from the repository root:

```bash
scripts/phase12/04-integration-test.sh --approved --admin-sudo
```

Confirmation: `TEST taskcompass_phase12_test WITH RUN-OWNED RESET`.
Use the Linux/sudo password if requested, then the Stage 1A password for
taskcompass_phase12_test. Do not disclose either password in conversation/logs.
The owner runs this interactive operation locally to supply private authentication;
the agent has not run it and no successful live validation is claimed. Await the
nonsecret output/exit result. On unexpected failure stop and preserve state; do not
rerun/reset/repair without reviewing it and obtaining any required recovery approval.

Only this milestone document changed. Markdown reference/fence and whitespace
checks passed. Source/tests were not rerun for this approval-record-only update.


## Linux Stage 1B unexpected failure and source correction — 2026-11-16

### Owner-reported live result — failed, state preserved

Owner ran the approved socket/sudo Stage 1B command on **feledria / MariaDB
10.5.29 / 3306 / Linux**. The wrapper reported successful empty fixed-target,
exact direct-grant, no-explicit-role and PUBLIC policy preflight. After the test
account password prompt, the executable reported:

```text
Unexpected migration failure before injection; preserve state for explicit review.
STOP: preserve Phase 1.2 test state; no automatic recovery/reset.
Organization integration test exit code: 1
```

This is **not a passed integration run**. The failure guard correctly avoided
cleanup of an unexpected failure. The agent has not rerun live tests, inspected SQL
on the server, modified retained state, executed migrations, changed grants/accounts
or accessed any other database. Stage 1B approval excludes automatic cleanup of
unexpected failures; a separate recovery decision is required. Preserve the current
taskcompass_phase12_test contents and output. No development migration is approved.

### Confirmed offline defect and changes

The schema-2 runner counted statements using QString::split(';', SkipEmptyParts).
The reviewed SQL ends in a semicolon followed by a newline; that newline is not
an empty string. The old expression therefore produces **10 segments** for 9 DDL
statements and fails the fixed manifest count before the first 002 DDL statement.
The old fault test swallowed this original exception and reported only that the
requested injected fault was not reached. The transcript lacks its original
exception/step, so other live issues cannot yet be ruled out.

Added the real runner statement-splitting path as MigrationRunner::migrationStatements
and regression assertions on the embedded resources (5 statements for 001, 9 for
002, no whitespace-only statements). With the old splitting behavior, the new
WorkspaceCodecTests assertion **failed offline** (exit 8, `Workspace codec/resource
assertion failed`). Then corrected the shared schema-2 splitting path to trim and
omit whitespace-only segments. The nine-statement manifest check remains enforced;
this is not a general-purpose SQL parser and does not support quoted semicolons or
stored programs. Migration SQL bytes, source checksums and schema are unchanged.

Updated the live fault-injection fixture to preserve/report the original safe
application diagnostic and intended injection index when failure precedes injection.
Migration DDL failures now report statement number plus numeric native MariaDB code
(or unavailable), without raw driver text, SQL values or credentials. Fail-closed
state preservation and the no-resume/no-repair policy remain unchanged.

Files changed by this correction:

- src/infrastructure/storage/MigrationRunner.h/.cpp: shared reviewed-DDL splitter,
  schema-2 usage and safe step/native-code failure diagnostic.
- tests/WorkspaceCodecTests.cpp: direct embedded statement-count regression.
- tests/OrganizationIntegrationTests.cpp: unexpected pre-injection cause reporting.
- scripts/phase12/inspect-failure.sql: four SELECT-only queries for the fixed
  Phase 1.2 target; no task text, authentication data, writes or recovery operations.
- docs/TestingPhase1.2.md: read-only retained-fixture inspection command.
- This milestone: failed result, source correction, evidence and next review gate.

### Retained state — expectation only, not observed

The first loop initializes and verifies schema 1 before attempting 002. If the
reported binary follows this source path, the statement-count rejection is before
MigrationAttempt CREATE, so expect the original five InnoDB tables, SchemaMigration
row 1, one Dataset row, no Task/CommandRecord/LocalChange rows and no MigrationAttempt.
No actual catalog/count/row state has yet been independently checked. Do not delete
objects based on this expectation; unexpected contents must be preserved/reviewed.

Read-only owner inspection command:

```bash
sudo mariadb --no-defaults --protocol=SOCKET --user=root \
   taskcompass_phase12_test < scripts/phase12/inspect-failure.sql
```

The inspection SQL is checked-in for exact review and contains only fixed-target
metadata/count SELECTs. No reset, migration, account or grant command is included.
If SchemaMigration/original tables are unexpectedly absent, the command can stop
with a query error; share that result and preserve state rather than improvising
repair. Actual output is needed before proposing concrete cleanup SQL.

### Validation and remaining checkpoint

Source-only builds/offline tests follow below. Both CMake live-test options remain
OFF; ordinary test runs do not connect. SQL 001/002 and Phase1.1.md remain unchanged;
no diagram regeneration is needed because the schema source is unchanged. Existing
backups and restored taskcompass_integration databases are untouched.

No cleanup/retry approval is requested yet: first inspect the retained state. Then
prepare exact target-specific recovery SQL and a renewed Stage 1B retry request
with the corrected build. Do not automatically resume migrations or rerun tests.

### Agent-executed offline results for the correction

```bash
cmake --build build --parallel 2
ctest --test-dir build -R '^WorkspaceCodecTests$' --output-on-failure
# Above regression failed before the fix; rebuild after applying it:
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
python3 docs/diagrams/GenerateDatabaseSchema.py --check
sha256sum migrations/mariadb/001_PersistentTaskFoundation.sql migrations/mariadb/002_OrganizationalManagement.sql docs/milestones/Phase1.1.md
git diff --check
```

Both builds succeeded. The initial new regression failed with the old split logic;
after the fix, full CTest passed **15 tests, 2 intentionally skipped, 0 failed**
(6.60 seconds). MariaDbIntegrationTests/OrganizationIntegrationTests skipped; the
live failure is not superseded by these offline results. Diagram/manifest check
passed (9 tables/85 columns/20 FKs/2 logical references). Migration 001/002 hashes
and accepted Phase1.1.md match their recorded protected values. Python checks passed
for modified Markdown references/fences and all four inspection statements being
SELECT-only and excluding the other TaskCompass database names. Final whitespace
check passed. No live SQL or recovery was executed by the agent.


## Retained Linux fixture inspection and recovery request — 2026-11-16

### Owner-reported read-only inspection

Owner executed scripts/phase12/inspect-failure.sql through the local socket client.
Reported server/database: **feledria / 10.5.29-MariaDB / 3306 /
taskcompass_phase12_test**. Exactly five BASE TABLE/InnoDB entries:
CommandRecord, Dataset, LocalChange, SchemaMigration and Task. MigrationAttempt
and organizational tables were absent from the table listing.

SchemaMigration contains row 1:

- checksum: `bb901d7580398893b5c04ee263f813b9d05873eb4bae08e0daf377faf73b0ea0`;
- recorded schema fingerprint: `6b3bd320ea92630b21909de0bd7cd38ea3bc3f644a69ba1fd249db43e656a343`;
- appliedAt: `1794878522914`.

Counts: **Dataset=1, Task=0, CommandRecord=0, LocalChange=0**. This matches the
source-based expectation: initial schema-1 synthetic baseline initialized before
the schema-2 statement-count rejection. This is owner-provided inspection evidence,
not a passed Stage 1B run or agent-executed SQL. The reported fingerprint is its
stored value; no separate current-DDL fingerprint recomputation is claimed.

### Approval Requested — reviewed fixture cleanup and corrected Linux Stage 1B retry

Request explicit authorization for this recovery only, on **feledria / MariaDB
10.5.29 / taskcompass_phase12_test**:

1. Stop/close all test writers and confirm retained contents still match the
   inspection above. If anything differs or additional objects/stored code are
   found, stop and review; this request does not cover unexpected contents.
2. Execute only the five statements below, in FK dependency order, without
   disabling foreign-key checks, --force, IF EXISTS, retries or general reset logic.
3. Verify the database remains present and entirely empty of schema objects.
4. Rebuild the corrected source, then rerun the previously reviewed Stage 1B suite
   using its normal strict preflight and test-account credentials. The suite's
   original migration hashes, fixed target and run-owned cleanup policy remain.

Exact proposed cleanup SQL, saved for review in
[scripts/phase12/reset-reviewed-baseline.sql](../../scripts/phase12/reset-reviewed-baseline.sql):

```sql
DROP TABLE taskcompass_phase12_test.LocalChange;
DROP TABLE taskcompass_phase12_test.CommandRecord;
DROP TABLE taskcompass_phase12_test.Task;
DROP TABLE taskcompass_phase12_test.Dataset;
DROP TABLE taskcompass_phase12_test.SchemaMigration;
```

The database, test account and all its grants remain. This discards the synthetic
Dataset row and schema-1 migration metadata recorded above; there are no reported
tasks/audit/change records to preserve. Do not drop or recreate the database/user.
No application data in taskcompass_dev or restored taskcompass_integration, no
other account/privilege and no backup/restore artifacts are included. No development
upgrade or Windows action is authorized by this request.

After approval, owner commands (not executed yet):

```bash
# Recheck; stop if contents differ from this reviewed baseline.
sudo mariadb --no-defaults --protocol=SOCKET --user=root \
   taskcompass_phase12_test < scripts/phase12/inspect-failure.sql
# Execute only after approval and matching inspection.
sudo mariadb --no-defaults --protocol=SOCKET --user=root \
   taskcompass_phase12_test < scripts/phase12/reset-reviewed-baseline.sql
```

Before cleanup, also run the following read-only object-count check and require
5/0/0/0 (tables/routines/triggers/events). Run it again after successful cleanup:

```sql
SELECT
   (SELECT COUNT(*) FROM information_schema.TABLES
      WHERE TABLE_SCHEMA='taskcompass_phase12_test') AS tablesRemaining,
   (SELECT COUNT(*) FROM information_schema.ROUTINES
      WHERE ROUTINE_SCHEMA='taskcompass_phase12_test') AS routinesRemaining,
   (SELECT COUNT(*) FROM information_schema.TRIGGERS
      WHERE TRIGGER_SCHEMA='taskcompass_phase12_test') AS triggersRemaining,
   (SELECT COUNT(*) FROM information_schema.EVENTS
      WHERE EVENT_SCHEMA='taskcompass_phase12_test') AS eventsRemaining;
```

After cleanup expect 0/0/0/0. The Stage 1B wrapper repeats empty-schema/encoding/grant/role/
PUBLIC checks before delegation. Then:

```bash
cmake --build build --parallel 2
scripts/phase12/04-integration-test.sh --approved --admin-sudo
```

DDL cleanup is not transactionally reversible. The recorded inspection preserves
this fixture's metadata evidence; no new backup is proposed because it contains
only disposable suite-generated baseline metadata and no task/history records.
Once deletion is approved and succeeds, the suite creates a new synthetic dataset
identity/history; this is not a restoration of the prior test Dataset UUID. The
accepted Phase 1.1 datasets and backup evidence are unaffected.

If any DROP/postcheck fails, stop and preserve remaining objects/output; partial
cleanup requires another explicit review, not automatic retry or completion.
Likewise a new unexpected suite failure preserves evidence and is not permission
to reuse this five-table cleanup blindly. Authorization is requested, **not yet
given or executed**. No live SQL was run by the agent in this continuation.

### Files and source-only validation

Changed this report/current status and docs/Roadmap.md; added the exact scoped
cleanup SQL file. C++ source, migrations, diagrams, validation scripts and Phase 1.1
history are unchanged. Static checks verify exactly the five fully qualified DROP
TABLE statements in dependency order; local Markdown references/fences and
git diff --check are validated. No database, build or CTest execution is necessary
for this documentation/SQL-proposal-only change; prior fixed-source offline results
remain 15 passed / 2 skipped and do not count as live validation.

Validation result: proposal static SQL ordering/scope, Markdown references/fences and git diff --check passed; no SQL executed.

## Reviewed Linux fixture cleanup/retry approved — 2026-11-16

Owner replied “Sure” to the exact request to remove the five reviewed tables from
taskcompass_phase12_test on Linux and retry Stage 1B with the corrected build.
This authorizes the stated recovery on feledria / MariaDB 10.5.29 only, conditional
on matching prechecks and no other writers/objects. All exclusions and failure
handling in the preceding concrete request remain binding. No additional approval
is needed for that exact operation. No cleanup/retry result has yet been reported.

Added scripts/phase12/check-object-counts.sql containing the exact read-only
object-count SELECT already shown in the recovery request. It makes the same
pre/post check easy to run without an interactive SQL session. No application,
migration, grant/account or cleanup SQL changes were needed. The owner executes
the commands locally for private sudo/test authentication; the agent has not run
live SQL or reset/retested the retained database.

### Approved owner walkthrough — run each step separately

Close all Phase 1.2 test writers. Stop on any error, unexpected result or changed
inspection; do not continue or automatically retry.

**1. Recheck the retained fixture.** Require the same five InnoDB tables, schema
row 1/checksum, Dataset=1 and Task/CommandRecord/LocalChange=0. Object counts must
be **5 / 0 / 0 / 0** (tables/routines/triggers/events).

```bash
sudo mariadb --no-defaults --protocol=SOCKET --user=root \
   taskcompass_phase12_test < scripts/phase12/inspect-failure.sql
sudo mariadb --no-defaults --protocol=SOCKET --user=root \
   taskcompass_phase12_test < scripts/phase12/check-object-counts.sql
```

**2. Perform the approved five-table cleanup.** Only if step 1 matches:

```bash
sudo mariadb --no-defaults --protocol=SOCKET --user=root \
   taskcompass_phase12_test < scripts/phase12/reset-reviewed-baseline.sql
cleanupExitCode=$?
printf 'Cleanup exit code: %s\n' "$cleanupExitCode"
```

Require exit 0; no command output otherwise is normal. Any failure means stop and
preserve the remaining state. Do not run the cleanup file a second time blindly.

**3. Verify empty state.** Require **0 / 0 / 0 / 0**:

```bash
sudo mariadb --no-defaults --protocol=SOCKET --user=root \
   taskcompass_phase12_test < scripts/phase12/check-object-counts.sql
```

**4. Build and retry Stage 1B.** The retained database/account/grants remain, so
do not rerun Stage 1A setup. Proceed only after successful steps 1–3:

```bash
cmake --build build --parallel 2
scripts/phase12/04-integration-test.sh --approved --admin-sudo
```

If the build fails, stop before invoking tests. Type
`TEST taskcompass_phase12_test WITH RUN-OWNED RESET` and enter the existing
taskcompass_phase12_test password when requested. Sudo may ask for the Linux
password. Share nonsecret output. A new unexpected failure needs a new review;
this recovery is not blanket approval for resetting subsequent unknown states.

Validation: static comparison confirms check-object-counts.sql is the same single
SELECT in the approved proposal and refers only to the Phase 1.2 schema. Markdown
links/fences and git diff --check passed. No build/tests or live SQL were run for
this SQL-helper/documentation-only approval record. Existing source-fix build and
offline results remain recorded above; live validation is pending, not passed.

Current Roadmap status also updated to approved recovery/retry with execution/results pending; final git diff --check passed.


## Corrected Linux Stage 1B passed — owner report, 2026-11-16

### Approved recovery actually executed by owner

Owner supplied the complete approved sequence on feledria:

1. check-object-counts.sql returned **5 / 0 / 0 / 0**.
2. reset-reviewed-baseline.sql completed with **cleanup exit code 0**.
3. check-object-counts.sql then returned **0 / 0 / 0 / 0**.
4. `cmake --build build --parallel 2 && scripts/phase12/04-integration-test.sh
   --approved --admin-sudo` proceeded. Build reported `ninja: no work to do.`

This is evidence of successful **owner-executed**, explicitly approved five-table
fixture cleanup. The test database/account/grants were retained. The historical
unexpected failure, its inspected metadata and all source-fix/recovery approvals
above remain part of the record; they are not overwritten by this success.

### Owner-reported Linux live validation

Server: **feledria / 10.5.29-MariaDB / 3306**, platform **Linux / Linux**.
Administrator Unix-socket/sudo preflight reported:

```text
PASS: empty fixed target, exact direct grants, no explicit roles, PUBLIC reviewed.
```

After authentication as taskcompass_phase12_test, the corrected suite reported:

```text
PASS: schema-1 preservation/upgrade, partial-migration refusal, typed commands/archive/history, atomic rollback, separate-connection lease timeout/loss/conflicting moves and restart. Phase 1.2 test schema left empty.
Organization integration test exit code: 0
```

Record **Linux Stage 1B passed**, based on the owner's transcript. This validates
the source-defined synthetic baseline/upgrade and deliberate interruption cases,
SQL ownership/constraint/history checks, command/receipt/change atomicity and
separate-connection scenarios on MariaDB 10.5.29. The connection-loss case is the
controlled lease-holder connection-close fixture previously documented, not an
unreported network-outage/uncertain-commit experiment. Restart is the repository's
new-connection test, not interactive GUI restart acceptance.

The suite reported its new Phase 1.2 schema empty at completion; no separate
post-suite SQL query is claimed. The owner had independently confirmed an empty
catalog immediately before the run. The agent executed no live SQL, cleanup,
account/grant operation or integration test in this reporting continuation.

### Current gates and next step

- Linux Stage 1A provisioning: passed (owner reported).
- Linux approved fixture recovery: passed (owner reported; pre/post counts and
  cleanup exit 0 recorded).
- Linux corrected Stage 1B synthetic integration: passed (owner reported; exit 0).
- Windows Phase 1.2 build/CTest and MariaDB 13 isolated provisioning/integration:
  **pending**, requiring the separate platform-specific live approval already
  specified. Accepted Phase 1.1 Windows evidence does not validate Phase 1.2.
- No development database upgrade/grant changes, actual-data backup/rehearsal,
  persistent organizational GUI/manual/restart testing, expanded-schema restore,
  Windows execution or milestone acceptance is authorized/implied by this result.
- Preserve all accepted Phase 1.1 data/backups/restored databases. No Phase 1.3.

Recommended next step: on Windows/UCRT64, run the database-free
`scripts/phase12/01-build.sh` and report its result. Then separately approve the
new Windows test target's provisioning and integration operations before live work.
Both MariaDB 10.5 and 13 isolated validations must precede any development upgrade.

### Documentation changes and validation

Updated current status in this milestone and docs/Roadmap.md; updated stale
source-only/not-yet-authorized wording in docs/TestingPhase1.2.md to distinguish
completed Linux operations from pending Windows gates. No application, migration,
scripts or SQL files changed. Markdown references/fences and git diff --check
passed. Builds/CTest/live queries were not rerun for this documentation-only
update; owner-executed results above remain clearly distinguished from agent checks.
No commits, pushes, merges or tags.


## Windows build succeeded; offline portability corrections — 2026-11-16

### Owner-reported Windows result

Owner ran scripts/phase12/01-build.sh under Windows UCRT64. Configure/generate
completed and all 75 build steps completed, ending with WorkspaceCodecTests.exe.
Record **Windows Phase 1.2 compilation passed**, not the complete offline gate.
CTest: **13 passed, 2 failed, 2 deliberately skipped**, 17 total, 59.86 seconds.
Failures: DatabaseSchemaTests and Phase12ValidationScripts. Both live integration
executables skipped. Existing task/domain/organization/commands/storage/Widgets
checks passed. No Windows live database operation was reported or authorized.

DatabaseSchemaTests failed comparing checked-in Markdown with generated Markdown;
the trace displayed em/en dashes inconsistently (replacement-character output).
The generator/test used Path.read_text without an explicit encoding. Python source
literals are UTF-8, but Windows file I/O can use a legacy locale code page. This
can also make console-rendered assertion diffs misleading. The exact owner locale
was not queried; a CP1252-default file-read simulation reproduced the same stale
Markdown assertion in the actual generator checker on Linux.

Phase12ValidationScripts stopped immediately after the 12-character boundary
fixture, before its socket/sudo section completed. The old test attempted to fake
a Linux uname on Windows and require Linux-only socket/sudo success there. Its
captured failure had no useful diagnostic; the exact failing Windows command is
not independently established. Native Linux-only positives do not belong in a
foreign-kernel simulation. Production socket checks remain unchanged.

### Source corrections and file inventory

- docs/diagrams/GenerateDatabaseSchema.py: explicit UTF-8 for all text reads and
  Mermaid subprocess text I/O. Generated Markdown/SVG/manifest writes use explicit
  UTF-8 bytes, preserving LF without Windows locale/newline substitution. This
  remains compatible with the existing Python 3.9 tooling; no new dependency.
- tests/DatabaseSchemaTests.py: explicit UTF-8 reads and a fifth unittest exercising
  the real `--check` path with CP1252 as the implicit default. It failed before
  the fix (`AssertionError: Stale Markdown`) and passes afterward. This tests actual
  comparison behavior rather than requiring a global PYTHONUTF8 workaround.
- tests/Phase12ValidationScripts.test.sh: Linux socket/sudo positive fixtures run
  only when the actual host reports Linux. Removed the in-test Linux uname spoof.
  Windows continues running TCP/setup/preflight/target/grant/PUBLIC/password/error/
  exit-code cases and explicit socket rejection; only unsupported Linux-positive
  fixtures report SKIP. Fake sudo is still installed for all branches so the mock
  suite cannot accidentally elevate a real command. Added safe line/exit/captured
  diagnostic reporting with fixture-secret redaction and no raw shell-command echo.
- docs/TestingPhase1.2.md: focused Windows rerun command and platform coverage note.
- This milestone and docs/Roadmap.md: current Windows build/test status.

The new ERR trap initially intercepted the deliberately tested exit-code 1 case.
Replaced its old set +e pattern with an explicit if/else capture so expected
nonzero statuses remain test data, while unexpected failures report their line.
Final Linux and simulated non-Linux branches pass. This local development failure
is corrected and does not change production script failure/approval behavior.

No C++ application code, migration SQL, schema manifest, generated diagram artifact,
production provisioning/testing script, permission or authentication policy changed.
The standalone SVG/Markdown remain byte-for-byte unchanged, so no renderer/visual
regeneration was needed. Actual Windows success after these fixes remains pending;
local branch simulations are not Windows execution evidence.

### Agent-executed validation

```bash
# Added CP1252 regression first: failed before fixing file encodings.
python3 tests/DatabaseSchemaTests.py
bash -n tests/Phase12ValidationScripts.test.sh
bash tests/Phase12ValidationScripts.test.sh
python3 docs/diagrams/GenerateDatabaseSchema.py --check
scripts/phase12/01-build.sh
git diff --check
sha256sum migrations/mariadb/001_PersistentTaskFoundation.sql migrations/mariadb/002_OrganizationalManagement.sql docs/milestones/Phase1.1.md
```

After correction, all five Python unittests passed. Generator/manifest/diagram
check passed (9/85/20/2). Native Linux shell mock suite passed. Additionally ran
that same suite with a temporary PATH-first uname stub printing MINGW64_NT_mock;
both Linux-positive sections reported SKIP and all remaining supported checks
passed. The stub was created in a TemporaryDirectory by Python, inherited PATH
was passed only to the test child, subprocess.run(check=True) verified exit 0,
and the temporary fixture was removed. No SQL/client/sudo beyond fakes ran.

The full numbered Linux build/offline route configured both live test flags OFF,
built successfully (`ninja: no work to do`) and passed **15 tests, 2 intentionally
skipped, 0 failed**, 6.68 seconds. Both integration binaries skipped; this does not
rerun or replace the earlier owner-reported Linux live validation. Protected SQL
001/002 and Phase1.1.md hashes match previous evidence. Final documentation link/
fence and whitespace checks are recorded below. No live SQL, provisioning/reset,
backup/restore, dependency installation, commit/push/merge/tag occurred.

### Required owner Windows retest — offline only

Update the Windows checkout with these source fixes. These two tests load Python/
Bash source directly, so a C++ rebuild is not required for the focused retry:

```bash
ctest --test-dir build-ucrt64 -R '^(DatabaseSchemaTests|Phase12ValidationScripts)$' --output-on-failure
```

Share the nonsecret result. A full scripts/phase12/01-build.sh rerun is also valid.
Do not proceed to Windows live provisioning/testing until the offline failures are
resolved and the separate platform-specific approvals are given. Existing Linux
approvals do not cover Windows; no development upgrade or milestone acceptance is
implied. No unresolved organizational architecture decision was introduced here.

Final consistency checks: all three modified Markdown files passed local-reference
and code-fence validation; git diff --check passed. No schema artifacts or historical
Phase 1.1 evidence were modified. Stop for the owner's focused Windows retest;
no additional live operation is authorized by these portability corrections.


## Windows offline retest passed — owner report, 2026-11-16

Owner ran:

```bash
ctest --test-dir build-ucrt64 -R '^(DatabaseSchemaTests|Phase12ValidationScripts)$' --output-on-failure
```

Actual reported results:

```text
DatabaseSchemaTests         Passed  0.91 sec
Phase12ValidationScripts    Passed  6.20 sec
100% tests passed out of 2
Total Test time (real) = 7.12 sec
```

Record both prior Windows failures as resolved. Windows compilation and offline
checks now pass **across the original 17-entry run (13 pass / 2 fail / 2 skip) and
this focused two-test retest**, not a newly executed full 17-entry pass. No Windows
live integration, new database setup or current server query is claimed. Original
failure/portability corrections remain preserved above. The agent performed no
build, test, SQL or provisioning in this documentation-only reporting turn.

### Approval Requested — Windows Stage 1A provisioning only

The next checkpoint is a separate Windows Stage 1A approval. Proposed target:
Windows MariaDB server at **127.0.0.1:3306** (expected 13.0.2-MariaDB, previous owner
hostname feldria-w10; actual identity must be reviewed from preflight). Fresh
database/account **taskcompass_phase12_test**, account host **127.0.0.1**.
Linux's completed approvals/results do not authorize this Windows operation.

Exact SQL (unchanged shared provisioning template):

```sql
SET SESSION sql_mode='NO_BACKSLASH_ESCAPES';
CREATE DATABASE taskcompass_phase12_test
   CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER 'taskcompass_phase12_test'@'127.0.0.1'
   IDENTIFIED BY '<distinct-12+-character-test-password>';
GRANT SELECT, INSERT, UPDATE, CREATE, DROP, ALTER
   ON `taskcompass\_phase12\_test`.*
   TO 'taskcompass_phase12_test'@'127.0.0.1';
```

The administrator uses Windows TCP/password authentication; no sudo/socket flag.
Preflight prints server hostname/version/port/platform, requires expected port and
MariaDB series, and requires zero existing database/same-name user counts before
creation. Review the actual host/version before supplying the new account password;
stop on any unexpected identity/version. Existing resources are not reused,
changed or repaired. Runtime/diagnostic/migration/Phase 1.1 test accounts, existing
taskcompass_dev data and restored taskcompass_integration remain outside scope.
No application tables or migration execution is included.

After explicit Windows Stage 1A approval, owner command in UCRT64:

```bash
scripts/phase12/03-setup-test.sh --apply --approved
```

Confirm `CREATE taskcompass_phase12_test`; provide the Windows MariaDB administrator
password, then a distinct new test-account password twice (12+ characters). Keep
passwords local/private. Source template/helper never stores them in the repository.
CREATE DATABASE/USER/GRANT is not atomic: if any operation fails, stop and preserve
output/resources; inspect before requesting exact recovery. No automatic retries,
DROP/reset, password repair, grant changes to existing accounts or dependency
installation is authorized. If resources already exist, report that and stop.

Stage 1B Windows live migrations/destructive synthetic integration requires another
explicit approval after provisioning/preflight review; development upgrades,
backup/restore, GUI acceptance and milestone acceptance remain later gates.

### Documentation validation

Changed current status in this milestone, docs/Roadmap.md and
docs/TestingPhase1.2.md, preserving chronological Windows failure and retest evidence.
Local Markdown reference/fence checks and git diff --check passed. No application,
SQL template, migration, script or test files changed; no executable checks needed
for this documentation-only approval request. No Git operations beyond read/diff.

## Windows Stage 1A approved — 2026-11-16

Owner explicitly approved the preceding Windows Stage 1A provisioning request.
Authorization covers only the new taskcompass_phase12_test database and dedicated
taskcompass_phase12_test@127.0.0.1 account on the intended Windows MariaDB server
at 127.0.0.1:3306, with the exact six scoped privileges and preconditions above.
Stop if existing resources or unexpected server identity/version are found; no
repair/overwrite is authorized. Stage 1B, development upgrades, existing-account
changes, backups/restores and other databases remain outside this approval.

Owner command in the Windows UCRT64 terminal:

```bash
scripts/phase12/03-setup-test.sh --apply --approved
```

Type `CREATE taskcompass_phase12_test`. Enter the Windows MariaDB administrator
password, review reported host/version/port, then enter a distinct new test-account
password twice (12+ characters). No sudo/socket option applies on Windows. Keep
passwords private; share only nonsecret result/error output. Stop after an error,
with no blind retry, and preserve partial state if provisioning began.

Status: authorization recorded; owner execution/result pending. The agent has not
executed provisioning, live SQL or database inspection. Only this milestone file
changed in this turn; local-link/fence and git diff --check validation passed.
No executable checks were rerun for this documentation-only approval record.


## Windows Stage 1A provisioning succeeded — owner report, 2026-11-16

Owner ran the approved command in UCRT64:

```bash
scripts/phase12/03-setup-test.sh --apply --approved
```

Reported server: **feldria-w10 / 13.0.2-MariaDB / port 3306**; platform
**MINGW64_NT-10.0-19045 / UCRT64**. Exact CREATE confirmation was entered and the
new 12+ character test-account password supplied twice. The helper reported:

```text
Created the isolated integration database/account with six scoped privileges.
No migrations/tests ran. Review grants and inherited policy before running tests.
```

Record **Windows Stage 1A passed (owner-reported)** for taskcompass_phase12_test /
taskcompass_phase12_test@127.0.0.1. No migration or integration result is implied;
exact effective privilege and empty-catalog verification remains Stage 1B preflight.
The client emitted its previously observed MariaDB 13 warning before calls:

```text
WARNING: option --ssl-verify-server-cert is disabled, because of an insecure passwordless login.
```

It did not stop provisioning. The helper did not change TLS options, root password,
authentication plugin or existing-account grants. No actual TLS-negotiation state
was measured here; the warning alone is not such a measurement. No action to
silence it by weakening verification is proposed. Existing Phase 1.1 development,
restored integration databases and backup artifacts remain outside this operation.

### Approval Requested — Windows Stage 1B isolated integration tests

Request separate approval for **feldria-w10 / MariaDB 13.0.2 / UCRT64**, test target
**127.0.0.1:3306 / taskcompass_phase12_test**, dedicated TCP account
**taskcompass_phase12_test@127.0.0.1**. This is not covered by Windows Stage 1A or
Linux execution approvals.

Authorized scope requested:

1. Windows TCP/password administrator **read-only** preflight: server identity,
   completely empty expected-encoding catalog, exact six scoped direct grants,
   no unexpected global/table/column/role grants or effective PUBLIC write/DDL
   authority. Unrelated PUBLIC test/test_% grants are accepted. Unknown state stops.
2. Execute the corrected OrganizationIntegrationTests using the unprivileged OS
   process and dedicated test account, creating synthetic schema-1 fixtures and
   exercising migration 002, ownership/constraints/receipts, rollback, restart,
   separate-connection mutation leases and deliberate interruption cases.
3. Only the suite's run-owned known-object fixture cleanup after empty-start proof;
   no adoption/reset of pre-existing objects. An intentional interruption must
   actually be reached before its cleanup. Unexpected failure preserves state
   and needs explicit review/recovery before retry.

The exact DDL is unchanged migration 001 plus section 6's nine statements in 002:

- 001 source SHA-256: `bb901d7580398893b5c04ee263f813b9d05873eb4bae08e0daf377faf73b0ea0`.
- 002 source SHA-256: `7de311935843e4fc04cbca74d025d9f38986a4154d70bdd29b35a2820f87b736`.

Source files are migrations/mariadb/001_PersistentTaskFoundation.sql and
migrations/mariadb/002_OrganizationalManagement.sql. Metadata prepared statements
and synthetic fixture operations are documented in the earlier Stage 1B preview
and tests/OrganizationIntegrationTests.cpp. The only run-owned cleanup statements
are the reviewed dependency order (server-normalized table identifier case may vary):

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

These are synthetic test resets, never a general migration recovery mechanism.
Existing baseline-reset-reviewed SQL is specific to its earlier Linux failure and
must not be reused for an unexpected Windows failure. DDL cannot be assumed to
roll back: on failure preserve catalog/history/attempt/output, inspect read-only,
then obtain exact new recovery approval. No automatic resume/repair/reset/retry.
No backup is proposed for the initial completely empty new target; no real task
records or historical Phase 1.1 data are adopted. Successful suite exit 0 reports
this new schema empty; exit 1 fails, 77 skips and is not a live pass.

After explicit Windows Stage 1B approval, owner command in UCRT64:

```bash
scripts/phase12/04-integration-test.sh --approved
```

Confirm `TEST taskcompass_phase12_test WITH RUN-OWNED RESET`. Enter the MariaDB
administrator password for read-only preflight, then the **new test-account
password created in Windows Stage 1A**. No sudo/socket option. Keep credentials
private and share nonsecret result/error output. This approval request includes no
account/grant changes, other databases, development upgrade, backup/restore, GUI
acceptance or milestone acceptance.

### Reporting-only changes and validation

Updated this milestone and current Roadmap/TestingPhase1.2 status. No source, SQL,
migration, script, account or database was changed by the agent. Local Markdown
references/fences and git diff --check passed; no application build/test rerun is
needed for this documentation-only result and approval request. No commit/push.

## Windows Stage 1B approved — 2026-11-16

Owner explicitly approved starting Windows Stage 1B. Authorization is the exact
preceding request: feldria-w10 / MariaDB 13.0.2 / UCRT64, isolated database/account
taskcompass_phase12_test at 127.0.0.1:3306, read-only administrator preflight and
synthetic suite/migrations/run-owned cleanup. All fresh-start, privilege, identity,
known-object and deliberate-fault guards remain. No other database, account/grant
change, development upgrade, backup/restore, unexpected-state reset or milestone
acceptance is authorized.

Owner command in UCRT64:

```bash
scripts/phase12/04-integration-test.sh --approved
```

Confirmation: `TEST taskcompass_phase12_test WITH RUN-OWNED RESET`. Enter the Windows
MariaDB administrator password for read-only checks, followed by the dedicated
taskcompass_phase12_test password created in Windows Stage 1A. No sudo/socket flag.
Keep passwords private. Share nonsecret output and stop on unexpected failure;
preserve state and do not rerun/reset automatically. Successful suite exit 0
reports the new test schema empty; neither approval nor a skipped run is a pass.

Status: approved, owner execution/result pending. The agent executed no live SQL,
preflight or tests in this turn. Only this milestone approval record changed;
Markdown references/fences and git diff --check passed. No application build/test
rerun needed for this documentation-only update; no commit/push/merge/tag.


## Windows Stage 1B passed — owner report, 2026-11-16

Owner ran the explicitly approved UCRT64 command:

```bash
scripts/phase12/04-integration-test.sh --approved
```

Server **feldria-w10 / 13.0.2-MariaDB / 3306**; platform
**MINGW64_NT-10.0-19045 / UCRT64**. The owner entered the exact test confirmation,
administrator preflight credentials and the dedicated Windows test-account password.
Read-only preflight reported:

```text
PASS: empty fixed target, exact direct grants, no explicit roles, PUBLIC reviewed.
```

The integration executable reported:

```text
PASS: schema-1 preservation/upgrade, partial-migration refusal, typed commands/archive/history, atomic rollback, separate-connection lease timeout/loss/conflicting moves and restart. Phase 1.2 test schema left empty.
Organization integration test exit code: 0
```

Record **Windows Stage 1B passed (owner-reported)**. The suite's source-defined
synthetic baseline/upgrade, deliberate interruption/refusal, SQL constraint/ownership,
commands/audit/change atomicity and separate-connection scenarios succeeded on
MariaDB 13.0.2. Its final message reports the new Phase 1.2 test schema empty;
no independent post-suite query or agent-executed SQL is claimed. Restart refers
to a new repository connection; connection-loss coverage is controlled lease-holder
closure, not an unreported interactive GUI/network-outage experiment.

The MariaDB 13 client warning about ssl-verify-server-cert appeared repeatedly
on administrator CLI calls. It did not prevent successful preflight or integration.
No TLS/client/authentication settings were changed to suppress it, and actual
TLS-negotiation status was not independently measured. No credentials were shared
in the owner's transcript or added to the repository.

### Completed isolated platform gate and remaining work

| Check | Linux / MariaDB 10.5.29 | Windows / MariaDB 13.0.2 |
| --- | --- | --- |
| Source build and offline checks | Agent-executed passing results above | Owner-reported compile pass and offline pass across full initial run plus two-test retest |
| New isolated target provisioning | Owner-reported Stage 1A passed | Owner-reported Stage 1A passed |
| Synthetic migration/integration suite | Owner-reported corrected Stage 1B passed after approved recovery | Owner-reported Stage 1B passed, exit 0 |
| Final new-test schema state | Suite reported empty | Suite reported empty |

Both server-version isolated validation prerequisites are now met. This does not
approve any taskcompass_dev upgrade or change to existing accounts/grants. Existing
accepted Phase 1.1 development data, restored taskcompass_integration databases,
private backups/exports and historical reports remain outside this test scope.
No repeated test/reset, migration resume/repair or Phase 1.3 work is implied.

Next prepare the separate development-upgrade review package: fresh read-only
baseline/identity/grant inspection, private populated backup and recovery/rehearsal
plan, actual-data preservation projections, exact temporary migration grants,
exact runtime grant additions/revocations and operator procedures. Section 7's
narrow per-table migrator/runtime privileges are still **proposed**: synthetic
integration used the six database-scoped test privileges, so it did not prove
those narrower role boundaries. Their validation/rehearsal must remain explicit
before relying on them for development execution. No privilege broadening is
silently authorized by the integration pass.

Separate approval remains required for each live next stage, including backup/
restore targets and exact artifacts, account/grant changes and development DDL.
Then real persistent organizational GUI/undo/redo/owner/archive/restart acceptance
and expanded-schema backup/restore checks remain on both platforms. Phase 1.2 is
not complete/accepted; no application launch against schema 1 is recommended.

### Documentation-only changes and validation

Updated current status in this milestone, docs/Roadmap.md and
docs/TestingPhase1.2.md, preserving all earlier failures, approvals and validation
evidence. No source/migration/scripts/schema/permissions or database contents were
changed by the agent. Markdown local-link/code-fence checks and git diff --check
passed. No build/test/live queries were rerun for this reporting-only update;
owner-reported live results are distinguished above. No commit/push/merge/tag.
