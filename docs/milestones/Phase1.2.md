<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Phase 1.2 — Organizational Management

**Status: Stage 1B isolated integration, B7.1 read-only development preflight, B7.2 populated schema-1 capture/private review and B7.3 populated restore/full preservation checks passed on Linux/MariaDB 10.5.29 and Windows/MariaDB 13.0.2 (owner-reported, 2026-11-17). All exact source snapshots/evidence are retained; both populated rehearsal clones are now schema2. B7.4a narrow-migrator grant transition and clone-only migration002 approved for operator execution on both platforms (2026-11-17); all four stages passed on Linux and Windows (owner-reported, 2026-11-17). No live database procedure performed by agent. B7.4b runtime-role rehearsal approved for operator execution on both platforms; Linux all three stages passed (owner-reported, 2026-11-17); Windows B7.4b remains pending. Development upgrade, GUI/restart and schema2 backup/restore remain separately gated. Phase 1.2 is not accepted.**
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


## 2026-11-16 — B7 development migration and acceptance review package

### Status, scope and authorization

**Prepared for review only.** The owner confirmed both Stage 1B isolated platform
passes and requested preparation of B7. No B7 live SQL, connection, provisioning,
grant change, backup/export capture, restore or migration was executed by the
agent. No such operation is authorized by this package. Phase 1.2 remains pending
validation and owner acceptance; Phase 1.3 is excluded. All previous reports,
failures, approvals and platform evidence above remain historical evidence.

The detailed operator procedures are in [Phase1.2B7.md](../Phase1.2B7.md).
This milestone remains the authoritative approval/result ledger. Each later
approval must identify platform/host, exact database/account, SQL/hash/artifact,
expected changes, stop conditions and recovery scope. Nothing below approves a
combined end-to-end run or a cleanup/reset.

### Inspected infrastructure and design findings

Reviewed existing Phase 1.1 provisioning/validation/backup/restore wrappers and
scripts; Phase 1.2 setup, integration preflight and identity guards;
DatabaseConfig/Connection, migration CLI/runner, migrations 001/002, structural
manifest and cumulative diagrams; persistent application identity/schema guards.
The authoritative design remains Requirements/Architecture/DataModel/Interactions
and Roadmap; no organizational cardinality, application behavior or schema changes.

- The current runner supports a populated 1→2 upgrade, explicit target version,
  migration/mutation locks, immutable migration-001 history, preservation checks and
  step journal. Any incomplete/uncertain attempt requires explicit review; no
  automatic suffix execution, reset, resume or metadata repair is proposed.
- Schema-1 verification must explicitly use `--verify --to-version 1` under the
  existing read-only migrator. Diagnostic credentials are valid for native
  SELECT/dumps, not Qt's guarded migration config. GUI requires schema 2 and the
  real runtime account on taskcompass_dev. These restrictions remain unchanged.
- Existing Phase 1.1 `09-backup.sh` can capture a new private **schema-1** baseline
  with Windows ACLs and five-table comparisons. Old setup/migration scripts expect
  empty development databases, and old restore/compare scripts use the retained
  original integration database: do not run them here. The old backup/export
  implementation does not validate all nine schema-2 tables.
- Both Stage 1B suites used six schema-wide privileges on synthetic data. They
  do **not** prove the proposed narrower roles or a real populated upgrade. The
  package adds a separately approved populated snapshot import and narrow-role
  rehearsal on taskcompass_phase12_test before any development DDL.
- The normal GUI cannot target that clone. Do not weaken its identity guard.
  Rehearsal runtime SQL checks roll back and are permission tests, not application
  command/history or GUI acceptance. Those real checks occur on development only
  after separately approved upgrade and runtime grants.
- A proposed new CLI-only taskcompass_phase12_restore target preserves both the
  populated rehearsal and the old restored taskcompass_integration. Its validation
  is nine-table data/DDL equality against an already Qt-verified source plus all
  FK/CHECK checks; no unsupported Qt verification is claimed there.

### Proposed stages and exact review material

| Stage | Deliverable and prerequisite | Separate approval / expected result |
| --- | --- | --- |
| B7.1 | Read-only development preflight on both known servers | First request below; schema 1/five tables, current checksums/fingerprint, dataset/counts and reviewed effective grants. |
| B7.2 | Fresh private populated schema-1 dump and canonical evidence | After preflight; new external path, stopped writers, dump exit 0/hash/private inspection and unchanged before/after data. Preserve all old artifacts. |
| B7.3 | Exact-file schema-1 restore into empty phase12 test target | Exact local dump hash, import SQL review and empty-target/grant preflight; same UUID and every legacy value/DDL definition. Retain clone. |
| B7.4a | Populated clone migration with narrowly scoped grants | Exact grant swap and unchanged 002; both MariaDB 10.5.29 and 13.0.2 must verify 2 and preserve all legacy data before dev upgrade. |
| B7.4b | Narrow runtime-equivalent clone role | Exact role swap plus rollback-only probe; exact grants, Qt read verification, full nine-table equality before/after probe. |
| B7.5 | Development temporary grants and 1→2 | Both rehearsals passed; fresh baseline and backup/hash revalidation. Explicit host-specific approval for exact grants and migration 002, stop on any drift. |
| B7.6 | Runtime extension and temporary migrator revocation | Preservation passed first; final 19 runtime table privileges, migrator/diagnostic SELECT only, runtime migration guard rejection. |
| B7.7 | Persistent GUI acceptance on both platforms | Deliberate labeled QA records; organizational/task ownership, archive inheritance/restoration, history/conflicts and restart checklist. |
| B7.8 | New private schema-2 populated snapshot | Fresh path/quiescence approval, all nine tables/85 columns and DDL evidence before/after capture. |
| B7.9 | Fresh CLI-only restore target and exact schema-2 import | Separate account/database provisioning, exact-artifact restore and later read-only grant conversion approvals. Full data/DDL equality and zero FK/CHECK violations. |
| B7.10 | Final review and owner acceptance | All actual platform/runtime/recovery results recorded. No automatic acceptance, cleanup, reset or next milestone. |

The runbook contains exact Bash invocations for Linux socket/sudo and Windows
UCRT64 password-authenticated administration, native clients/dump tools, existing
Qt CLI flags and scoped runtime GUI launch. Passwords are terminal prompts or
session-only Qt environment values, never saved. SQL templates contain no secrets.
The proposal SQL files do not execute automatically and have no new runner hook.

Exact migration grant delta (existing schema-wide SELECT retained), in
[migration-dev-grants.sql](../../scripts/phase12/b7/migration-dev-grants.sql):
CREATE on Pillar/Project/Objective; CREATE/INSERT/UPDATE on MigrationAttempt;
ALTER on Task/CommandRecord/LocalChange; INSERT on SchemaMigration. Eight GRANT
statements, no DROP, global/grant-option rights or audit payload UPDATE. CREATE and
ALTER cover indexes embedded in their DDL; no standalone CREATE INDEX is run.
Support for grants on absent tables and the narrow minima must be proved during
both populated rehearsals, not assumed. Any denial stops for review, not broadening.

Exact final runtime delta and matching migration revocations are in
[runtime-dev-grants-and-revoke.sql](../../scripts/phase12/b7/runtime-dev-grants-and-revoke.sql).
App gains SELECT/INSERT/UPDATE on each new organizational table and SELECT on
MigrationAttempt. Its final table privileges total 19: four mutable tables with
SELECT/INSERT/UPDATE, two append-only history tables with SELECT/INSERT, three
metadata tables with SELECT. Diagnostic and migrator retain their existing
SELECT-only schema grants/passwords. No existing account is recreated or reset.
Rehearsal role swaps model the same boundaries; new CLI-only restore account is
proposed with SELECT/INSERT/CREATE only, then separately revoke INSERT/CREATE.

### Preservation, recovery and acceptance criteria

Legacy export uses all **38 original columns**, including task IDs/owners (old
schema has no owner fields), lifecycle/tombstones, timestamps/revisions, Dataset
name/UUID, the entire migration-1 tuple and every CommandRecord/LocalChange column.
HEX/type/NULL-tagged output preserves raw JSON bytes, property order, whitespace,
Unicode, empty versus NULL and compensation/reapply links. Stable ID/version order
and all-row counts avoid misleading active-only or SELECT-* comparisons. New
organizational tables must be empty, all new legacy owner/target fields NULL,
payloads still version 1 and attempt 2 completed at step 9 before GUI activity.
The complete schema-2 export covers all **85 columns/nine tables**, both migration
rows and MigrationAttempt; matching same-server raw DDL proves indexes/constraints
alongside the runner fingerprint. Health SQL covers 20 FKs and every actual CHECK.

Backups require quiescence, fresh nonoverwriting external directories and verified
private permissions/Windows ACLs before data files. Same-platform native client/
server version is used; no cross-version dump editing or portability claim. New
schema-1 and schema-2 directory variables remain distinct. Every artifact/hash,
exit/result, actual dataset/count and effective grant observation must be recorded
privately or summarized without disclosing task data. A checksum alone is not a
SQL safety review or successful recovery test.

MariaDB DDL/grants are not a rollback bundle. Import/grant/migration failures may
leave partial state; stop, preserve it and obtain exact state-specific recovery
approval. Do not resume, overwrite backups or reset any target. A populated clone
restore proves recoverability without touching dev. Any actual in-place fallback
would require fresh forensic capture and separate destructive-operation approval
for the observed state and exact backup; no universal DROP/down migration is
prepared or authorized. Keep an accepted schema-1 build for any approved fallback;
the new GUI cannot run on schema 1. If schema-2 work exists, preserve it before
considering fallback. Table rebuild time/free-space requirements remain unmeasured.

GUI acceptance in the runbook covers legal ownership and parent-aware edits,
deep navigation, subtree moves/cycle rejection, views sharing UUIDs, explicit versus
inherited archive flags/locating archived ancestors, restore without child-flag
rewrites, chronological undo/redo and conflict rejection, original task behavior,
restart persistence and empty session undo history with durable receipts retained.
No completion/deletion lifecycle for organizations or Phase 1.3 controls are added.

### Files created or modified

- New [B7 operator review package](../Phase1.2B7.md), including all commands,
  exact SQL links, platform differences, checklist and stop/recovery matrix.
- New `scripts/phase12/b7/` with 19 **proposed** SQL files:
  `preflight-dev.sql`, `preflight-dev-grants.sql`, `preflight-rehearsal.sql`,
  `preflight-restore.sql`, `preflight-clone-grants.sql`, `public-grants.sql`;
  `legacy-export.sql`, `schema2-export.sql`, `ddl1-export.sql`, `ddl2-export.sql`,
  `schema2-health.sql`, `upgrade-postconditions.sql`;
  `migration-dev-grants.sql`, `runtime-dev-grants-and-revoke.sql`,
  `rehearsal-migration-grants.sql`, `rehearsal-runtime-grants.sql`,
  `runtime-probe.sql`, `restore-account.template.sql`, `restore-account-readonly.sql`.
- New [ReviewChecksums.sha256](../../scripts/phase12/b7/ReviewChecksums.sha256)
  pins the runbook/19 SQL files. From root, verification is read-only and offline.
- Updated this milestone's current status and append-only report,
  [Roadmap](../Roadmap.md) and [TestingPhase1.2](../TestingPhase1.2.md) pointers.
- No application source, migrations, schema diagrams, existing validation scripts,
  protected Phase 1.1 report, live database or private artifact was changed.

### Agent-executed offline validation and limitations

Commands from repository root:

```bash
python3 tests/DatabaseSchemaTests.py
python3 docs/diagrams/GenerateDatabaseSchema.py --check
sha256sum --check scripts/phase12/b7/ReviewChecksums.sha256
sha256sum migrations/mariadb/001_PersistentTaskFoundation.sql \
   migrations/mariadb/002_OrganizationalManagement.sql docs/milestones/Phase1.1.md
git diff --check
```

Results: all **5 schema tests passed**; cumulative artifacts/manifest consistent
(9 tables, 85 columns, 20 SQL FKs, 2 logical references). All 20 review checksum
entries passed. Protected hashes unchanged:
001 `bb901d7580398893b5c04ee263f813b9d05873eb4bae08e0daf377faf73b0ea0`;
002 `7de311935843e4fc04cbca74d025d9f38986a4154d70bdd29b35a2820f87b736`;
Phase1.1.md `ffbf123937642a3478662d817a4f522377bb0ae6271fec51810e5b735bc34bde`.
Markdown local links/fences and diff whitespace checks passed.

The additional offline structural review below was executed as
`python3 /tmp/taskcompass-b7-review.py`; its complete source is provided for
reproduction without a retained temporary file. It checks export column coverage,
all FK/CHECK coverage, proposed grant counts, balanced SQL statements and preflight/
probe statement boundaries, Bash syntax only (`bash -n`, never execution), and
Markdown references. An initial checker inventory assertion expected 20 SQL files;
the actual inventory is 19. Corrected that review-check bookkeeping and reran:
**PASS, 19 SQL files, 38/85 columns, 20 FKs/all CHECKs, 36 Bash blocks parse**.
This is structural/syntax review, not MariaDB SQL parsing or live permissions proof.

```bash
python3 - <<'PY'
from pathlib import Path
import importlib.util, json, re, subprocess
spec = importlib.util.spec_from_file_location('schema', 'docs/diagrams/GenerateDatabaseSchema.py')
schema = importlib.util.module_from_spec(spec)
spec.loader.exec_module(schema)
manifest = json.loads(Path('migrations/mariadb/SchemaManifest.json').read_text(encoding='utf-8'))
base = Path('scripts/phase12/b7')
assert len(list(base.glob('*.sql'))) == 19
for version, name, expected in [('1', 'legacy-export.sql', 38), ('2', 'schema2-export.sql', 85)]:
   source = (base / name).read_text(encoding='utf-8')
   actual = []
   for statement in schema.splitSql(source, ';'):
      if not re.search(r"SELECT '\w+' AS tableName", statement):
         continue
      table = re.search(r'FROM (\w+)', statement)[1]
      columns = re.findall(r"IF\((\w+) IS NULL", statement)
      assert columns == [c['name'] for c in manifest[version][table]['columns']], table
      actual.extend((table, column) for column in columns)
   assert len(actual) == len(set(actual)) == expected
migration = (base / 'migration-dev-grants.sql').read_text(encoding='utf-8')
assert len(re.findall(r'^GRANT ', migration, re.M)) == 8
runtime = (base / 'rehearsal-runtime-grants.sql').read_text(encoding='utf-8')
assert sum(len(rights.split(',')) for rights in re.findall(r'^GRANT (.*?) ON', runtime, re.M)) == 19
health = (base / 'schema2-health.sql').read_text(encoding='utf-8')
for table, value in manifest['2'].items():
   for name, *_ in value['foreignKeys']:
      assert f"'{name}' AS ruleName" in health
   for name, expression in value['checks']:
      assert f"'{table}.{name}' AS ruleName" in health and f'NOT ({expression})' in health
assert sum(len(t['foreignKeys']) for t in manifest['2'].values()) == 20
for path in base.glob('*.sql'):
   text = re.sub(r'^--.*$', '', path.read_text(encoding='utf-8'), flags=re.M)
   statements = schema.splitSql(text, ';')
   if path.name.startswith('preflight-') or path.name == 'public-grants.sql':
      assert all(re.match(r'^(SELECT|SHOW)\b', s) for s in statements), path
   assert 'GRANT OPTION' not in text and 'ON *.*' not in text
   if path.name == 'runtime-probe.sql':
      assert any(s == 'ROLLBACK' for s in statements)
      assert not any(re.match(r'^(COMMIT|CREATE|ALTER|DROP|DELETE|GRANT|REVOKE)\b', s) for s in statements)
      assert set(re.findall(r'\b(taskcompass_\w+)\.', text)) == {'taskcompass_phase12_test'}
runbook = Path('docs/Phase1.2B7.md').read_text(encoding='utf-8')
blocks = re.findall(r'^```bash\n(.*?)^```\s*$', runbook, re.M | re.S)
for block in blocks:
   subprocess.run(['bash', '-n'], input=block, text=True, check=True)
for path in [Path(p) for p in ['docs/Phase1.2B7.md', 'docs/milestones/Phase1.2.md', 'docs/Roadmap.md', 'docs/TestingPhase1.2.md']]:
   text = path.read_text(encoding='utf-8')
   assert len(re.findall(r'^```', text, re.M)) % 2 == 0, path
   text = re.sub(r'^```[^\n]*\n.*?^```\s*$', '', text, flags=re.M | re.S)
   for target in re.findall(r'\]\(([^)]+)\)', text):
      if '://' not in target and not target.startswith('#'):
         assert (path.parent / target.split('#')[0]).exists(), (path, target)
print(f'PASS: 19 SQL files structurally reviewed; 38/85 columns, 20 FKs/all CHECKs, readonly preflight, rollback-only probe; {len(blocks)} Bash blocks parse; Markdown links/fences.')
PY
```

No C++/CMake behavior changed, so compilation/full CTest were not rerun for this
review-only package. No Windows shell, PowerShell ACL execution, actual populated
snapshot, narrow-role grant, new restore target, GUI or migration was tested by
the agent. Existing owner-reported Stage 1B evidence is preserved, not relabeled as
new B7 results. Official MariaDB references and tool-version constraints are linked
in the runbook. No dependency installation, commit/push/merge/tag occurred.

### Approval Requested — B7.1 read-only preflight only

Request explicit approval for **both separately identified development servers**:

- Linux `feledria`, MariaDB 10.5.29, local 127.0.0.1:3306, taskcompass_dev.
- Windows `feldria-w10`, MariaDB 13.0.2, local 127.0.0.1:3306, taskcompass_dev.

Scope: run exactly [preflight-dev.sql](../../scripts/phase12/b7/preflight-dev.sql)
and [preflight-dev-grants.sql](../../scripts/phase12/b7/preflight-dev-grants.sql)
under local socket/sudo root on Linux or native password-prompt root on Windows;
Windows additionally [public-grants.sql](../../scripts/phase12/b7/public-grants.sql).
These are SELECT/SHOW only, scoped to TaskCompass metadata/grants; no authentication
hashes or task titles/notes are selected. Then existing SELECT-only migrator runs
`TaskCompassMigrate --verify --to-version 1` with the runbook's explicit dev env.
Qt connection setup uses session-only settings/locks; no persistent mutation or DDL.

Expected: known server/schema1/five InnoDB tables, no stored code, no other target
writers, checksum/fingerprint verified, one dataset, all counts recorded and exact
Phase 1.1 account rights/no unexpected effective PUBLIC/role authority. All values
are **unobserved for this new preflight**. Stop on discrepancy, connection failure,
unknown privilege or version mismatch and report; do not repair or rerun writes.
Recovery for this stage is inspection/reporting only because no database mutation
is proposed. Keep credentials private and redact sensitive diagnostic snippets.

This first approval does **not** include backup/export creation, imports, grant
changes, provisioning, migration, GUI writes, test resets or cleanup. After the
observations are reviewed, request B7.2 separately with a fresh private directory;
subsequent stages bind exact file hashes and targets when those artifacts exist.
No live B7 operation may begin until the matching explicit approval is received.


## 2026-11-16 — B7.1 approved; Linux operator authentication required

### Owner decision and current scope

Owner response "Yes, go ahead" approves the immediately preceding B7.1 request
for Linux and Windows read-only preflight. No new approval is required to finish
that exact stage. B7.2 backups, all later grants/provisioning/migrations/restores,
GUI writes and cleanup remain unauthorized. The earlier "Approval Requested"
section is the historical request; this entry records its approval.

### Agent-executed checks and failed connection attempt

`sha256sum --check scripts/phase12/b7/ReviewChecksums.sha256` passed all 20 entries.
`command -v mariadb` returned `/usr/bin/mariadb`; `hostname` returned `feledria`.
Attempted exactly:

```bash
sudo -n -- mariadb --no-defaults --protocol=SOCKET --user=root \
   --default-character-set=utf8mb4 --batch --raw --database=taskcompass_dev \
   < scripts/phase12/b7/preflight-dev.sql
```

The sandbox initially rejected sudo with `The "no new privileges" flag is set`.
The same approved read-only command was retried outside the sandbox through the
execution approval mechanism; sudo exited 1 with `sudo: a password is required`.
**Neither attempt connected or executed SQL.** No administrator password was
requested through chat, captured or stored. No alternate credentials were sought.
The grants query and Qt schema-1 verification were not attempted because the
metadata preflight did not run. Windows is unavailable in this Linux session.

### Next operator action within approved B7.1

On Linux, close TaskCompass clients/writers and run from repository root:

```bash
sudo -v
cat scripts/phase12/b7/preflight-dev.sql scripts/phase12/b7/preflight-dev-grants.sql |
   sudo -n -- mariadb --no-defaults --protocol=SOCKET --user=root \
      --default-character-set=utf8mb4 --batch --raw --database=taskcompass_dev
```

Enter the Linux OS password only at sudo's local terminal prompt. Stop on any
error or unexpected output; share nonsecret results for review. This runs only
the two approved SELECT/SHOW files. It creates no export/backup and changes no
accounts, grants or schema. Do not include task contents or credentials in output.
After metadata/grants pass review, finish the already approved explicit
`--verify --to-version 1` block in [B7.1](../Phase1.2B7.md#b71--read-only-preflight-first-approval-requested)
with the existing taskcompass_migrate password entered privately. No --apply.
Windows uses the corresponding B7.1 UCRT64 block, including PUBLIC review; do not
run that MariaDB 13 PUBLIC query on Linux 10.5.

### Files, validation and remaining work

Updated this milestone status/history and Roadmap current status only. Existing
review package, SQL, source, migrations and private artifacts are unchanged.
Markdown local references/fences and `git diff --check` passed; no application build
or CTest was rerun for this status-only change. Linux preflight, Qt fingerprint
verification and all Windows preflight results remain pending, not failed database
validation. No database write, backup, restore, reset, commit, merge, tag or push.
Do not request or execute B7.2 until actual B7.1 results are reviewed.


## 2026-11-16 — B7.1 Linux metadata/grants reviewed (owner-reported)

Owner executed the approved `sudo -v` and combined preflight-dev.sql /
preflight-dev-grants.sql invocation recorded above, then supplied its output.
This supersedes the pending Linux metadata observation, not the historical agent
sudo failure. No agent-executed live SQL is claimed. No explicit CLI exit code
was supplied; the transcript reaches the final expected runtime privilege rows
without a displayed SQL error.

| Observation | Owner-reported value / review |
| --- | --- |
| Server / selected database | feledria, 10.5.29-MariaDB, port 3306, taskcompass_dev |
| Administrator / server settings | root@localhost, lower_case_table_names=0, FK/CHECK enforcement both 1 |
| Encoding | utf8mb4 / utf8mb4_unicode_ci |
| Objects | Exactly five BASE TABLE/InnoDB: SchemaMigration, Dataset, Task, CommandRecord, LocalChange |
| Stored code / other sessions | No routine/trigger/event or other selected-target session rows displayed |
| Migration | Only version 1; checksum bb901d7580398893b5c04ee263f813b9d05873eb4bae08e0daf377faf73b0ea0 |
| Stored fingerprint | 6b3bd320ea92630b21909de0bd7cd38ea3bc3f644a69ba1fd249db43e656a343; actual current fingerprint still requires Qt verification |
| Migration appliedAt | 1794627756442 |
| Dataset | 21be9da7-b767-4578-b972-cc9a35122ddb, createdAt 1794627756441; count 1 |
| Counts | SchemaMigration 1; Dataset 1; Task 4; CommandRecord 28; LocalChange 28 |
| Task breakdown | 2 active/non-tombstoned; 1 active/tombstoned; 1 completed/non-tombstoned |
| Account hosts | app, diagnostic and migrator each only 127.0.0.1 in scoped report |
| Global / schema privileges | Each global USAGE, not grantable; diagnostic and migrator each exact escaped-schema SELECT only |
| Runtime table privileges | Exactly 9 rows: Task SELECT/INSERT/UPDATE; CommandRecord and LocalChange SELECT/INSERT; Dataset and SchemaMigration SELECT; none grantable |
| Extra columns / roles | No column privilege or role assignment rows displayed |

Supplied SHOW CREATE definitions match the expected schema-1 columns, keys,
constraints, engines and collations on inspection. This is not a replacement for
runner fingerprint verification. No unexpected privilege, data count or schema
object is apparent. The populated baseline includes a tombstoned task and 28
historical receipts/outbox rows; all must remain in subsequent exact comparisons.
No PUBLIC query is needed on this MariaDB 10.5 server. No mutation/grant/backup
approval follows from the metadata result.

### Next approved check and remaining work

Run from Linux repository root using the **existing taskcompass_migrate password**,
not root or the diagnostic password. This standalone form does not depend on the
runbook's shell variables and performs verification only:

```bash
(
   set +x
   export TASKCOMPASS_DB_HOST=127.0.0.1 TASKCOMPASS_DB_PORT=3306
   export TASKCOMPASS_DB_NAME=taskcompass_dev TASKCOMPASS_DB_USER=taskcompass_migrate
   export TASKCOMPASS_DB_ISOLATED=1
   IFS= read -r -s -p 'taskcompass_migrate password: ' TASKCOMPASS_DB_PASSWORD
   printf '\n'
   export TASKCOMPASS_DB_PASSWORD
   ./build/TaskCompassMigrate --verify --to-version 1
   verificationExitCode=$?
   unset TASKCOMPASS_DB_PASSWORD
   printf 'Verification exit code: %s\n' "$verificationExitCode"
   exit "$verificationExitCode"
)
```

Expected schema 1 / dataset 21be9da7-b767-4578-b972-cc9a35122ddb / exit 0.
Stop and report any failure; do not run --apply. Windows B7.1 metadata/grants/PUBLIC
and Qt verification remain pending. B7.2 and later operations are not authorized.
Updated this milestone and Roadmap status; Markdown reference/fence checks and
`git diff --check` passed. Build/CTest not rerun for reporting-only edits; source,
SQL review package, migrations and all database/backup artifacts unchanged by the
agent. No commit/push/merge/tag or Phase 1.3 work.


## 2026-11-16 — B7.1 stopped: CHECK constraint verification failure

### Actual owner-reported result

Owner ran the exact read-only migrator verification block recorded above using
`taskcompass_migrate` against taskcompass_dev. Output:

```text
CHECK constraints differ from registered migration.
Verification exit code: 1
```

**Linux B7.1 has not passed.** No backup, migration, grant or other repair is
approved by this failure. Windows preflight remains pending. Previously supplied
root SHOW CREATE and privilege observations remain preserved; they do not override
this failed verification. No agent-executed live SQL occurred during this review.

### Source-only investigation

Reviewed `MigrationRunner::requireShape` in MigrationRunner.cpp. This exact error
compares the sorted CHECK **names** returned by information_schema.TABLE_CONSTRAINTS
under the connected account against the embedded version-1 SchemaManifest. It
occurs before successful verification of the current DDL fingerprint. It is not
proof that a CHECK expression itself changed or that existing task data is invalid.

Offline manifest inspection shows 12 schema-1 CHECKs: CommandRecord commandPayload /
commandKind; Dataset datasetIdFormat / datasetCreatedAt; LocalChange changePayload /
changeState; Task taskIdFormat / taskTitle / taskNotes / taskLifecycle / taskTimes /
taskRevision; none on SchemaMigration. Those names all appear in the owner's prior
root SHOW CREATE output. Need compare TABLE_CONSTRAINTS visibility under root and
the SELECT-only migrator before diagnosing schema drift versus visibility or
verifier behavior. Neither cause is established. Do not add grants, weaken the
validator, edit migration history/fingerprints or reapply DDL speculatively.

### Proposed next read-only diagnostic (operator review)

The following adds no schema or data operation; it compares only selected-database
identity and CHECK-name catalog rows under the two known local accounts. It is
additional investigation of the B7.1 discrepancy, not a migration/recovery step.
Run from the Linux terminal and share both nonsecret result sets:

```bash
inspectionSql="SELECT DATABASE() AS selectedDatabase,CURRENT_USER() AS authenticatedAccount;
SELECT TABLE_NAME,CONSTRAINT_NAME,CONSTRAINT_TYPE
FROM information_schema.TABLE_CONSTRAINTS
WHERE CONSTRAINT_SCHEMA=DATABASE() AND CONSTRAINT_TYPE='CHECK'
ORDER BY TABLE_NAME,CONSTRAINT_NAME;"

sudo mariadb --no-defaults --protocol=SOCKET --user=root \
   --database=taskcompass_dev --batch --execute="$inspectionSql"

env -u MYSQL_PWD mariadb --no-defaults --protocol=TCP \
   --host=127.0.0.1 --port=3306 --user=taskcompass_migrate --password \
   --database=taskcompass_dev --batch --execute="$inspectionSql"
unset inspectionSql
```

The second password prompt is the existing taskcompass_migrate database password;
sudo uses the OS password if needed. No password is saved or printed. Stop on any
query error. Record whether both lists show exactly the same 12 CHECKs, and their
authenticatedAccount values; do not treat a missing list as permission to repair.
After these observations, propose a narrowly scoped source correction only if
supported by evidence; any new live privilege change remains separately gated.

### Files, validation and remaining work

Updated this milestone and Roadmap current status, preserving earlier observations.
Manifest inspection was offline Python reading SchemaManifest.json; no schema,
application source, SQL procedure, database or backup artifact was changed.
Markdown references/fences and `git diff --check` passed. No build/full CTest was
rerun because these are documentation-only updates. Investigation pending; no B7.2
approval requested, no Phase 1.3 or commit/push/merge/tag.


## 2026-11-17 — CHECK catalog visibility difference confirmed

### Owner-reported evidence and conclusion

Owner ran the proposed identity/CHECK-name SELECTs. Root reported selected database
`taskcompass_dev`, authenticated account `root@localhost` and all 12 expected CHECKs:

| Table | CHECK names visible to root |
| --- | --- |
| CommandRecord | commandKind, commandPayload |
| Dataset | datasetCreatedAt, datasetIdFormat |
| LocalChange | changePayload, changeState |
| Task | taskIdFormat, taskLifecycle, taskNotes, taskRevision, taskTimes, taskTitle |

The TCP account reported the same selected database and authenticated account
`taskcompass_migrate@127.0.0.1`, followed by **no CHECK rows** and no displayed SQL
error. No explicit CLI exit code was supplied. These are owner observations, not
agent-executed SQL. They confirm a difference in catalog visibility under the
approved SELECT-only account, sufficient to explain the verifier's failed CHECK
name comparison. Missing catalog rows must not be interpreted as missing actual
constraints; root's SHOW CREATE and CHECK-name results show the expected names.
Full DDL fingerprint verification still has not passed, so no blanket schema/data
integrity or migration-readiness claim is made.

Source review also found FK-name checks using TABLE_CONSTRAINTS, FK-column checks
using KEY_COLUMN_USAGE, and index checks using STATISTICS. Merely bypassing the
CHECK comparison could conceal drift or fail at the next catalog. No bypass,
extra privileges or fingerprint rewrite is acceptable. The proposed correction
should obtain authoritative structural information accessible to least-privilege
accounts (potentially SHOW CREATE TABLE), retain exact registered shape/FK endpoint
checks and the unchanged full DDL fingerprint, and fail closed for inaccessible,
malformed or unexpected definitions. Need confirm that account's SHOW CREATE and
remaining catalog visibility before choosing the smallest safe source correction.
No architectural change or source patch has been implemented in this report.

### Next read-only inspection, no new database authority

One TCP prompt under the same read-only migrator can collect those metadata facts:

```bash
env -u MYSQL_PWD mariadb --no-defaults --protocol=TCP \
   --host=127.0.0.1 --port=3306 --user=taskcompass_migrate --password \
   --database=taskcompass_dev --batch --execute="
SELECT DATABASE(),CURRENT_USER();
SHOW CREATE TABLE CommandRecord;
SELECT TABLE_NAME,CONSTRAINT_NAME,COLUMN_NAME,REFERENCED_TABLE_SCHEMA,
       REFERENCED_TABLE_NAME,REFERENCED_COLUMN_NAME,ORDINAL_POSITION
FROM information_schema.KEY_COLUMN_USAGE
WHERE CONSTRAINT_SCHEMA=DATABASE()
ORDER BY TABLE_NAME,CONSTRAINT_NAME,ORDINAL_POSITION;
SELECT TABLE_NAME,INDEX_NAME,COLUMN_NAME,NON_UNIQUE,SEQ_IN_INDEX
FROM information_schema.STATISTICS
WHERE TABLE_SCHEMA=DATABASE()
ORDER BY TABLE_NAME,INDEX_NAME,SEQ_IN_INDEX;"
```

This selects schema metadata only, no task/history payloads or secrets. Enter only
the existing taskcompass_migrate password at its local prompt; stop on errors.
Do not run --apply, any GRANT or recovery operation. Subsequent source regression
coverage must include metadata hidden from SELECT-only accounts and rejection of
missing/extra/changed constraints, FK endpoints and fingerprint drift. Broader
synthetic integration passes did not test this read-only visibility condition.

### Reporting and remaining work

Updated milestone current status/history and Roadmap to reflect confirmed
visibility rather than speculative missing constraints. No source, schema,
privilege, database, backup or proposed SQL file was modified. Markdown references/
fences and `git diff --check` passed; application build/CTest not rerun for these
reporting-only edits. B7.1 remains blocked; Windows preflight and B7.2 onward pending.
No live agent SQL, commit/push/merge/tag or Phase 1.3 work.


## 2026-11-17 — least-privilege verifier compatibility correction

### Owner metadata evidence

The owner supplied the requested TCP metadata inspection under the existing
SELECT-only migrator. SHOW CREATE TABLE CommandRecord succeeded and included the
expected commandTask FK and commandPayload/commandKind CHECKs. KEY_COLUMN_USAGE
returned primary/unique keys and ordered FK columns/endpoints within
taskcompass_dev; STATISTICS returned the expected primary, unique and secondary
index columns/uniqueness. This supports using SHOW CREATE for constraint names
without adding grants. The CHECK-name catalog remains empty for that account as
previously reported. Output is owner-reported; no new CLI exit code was provided.

### Implemented source-only correction within Step B authorization

- MigrationRunner now obtains sorted named CHECK/FK constraints from SHOW CREATE
  TABLE via a focused storage-adapter helper instead of relying on the
  privilege-filtered TABLE_CONSTRAINTS catalog. No empty-result bypass or fallback
  to accepting missing constraints. Expected names must still match exactly.
- New SchemaConstraints helper accepts the observed MariaDB quoted, one-definition-
  per-line output; duplicate names, unsupported kinds/formatting or absent DDL
  fail closed. Identifier escapes and CRLF are supported. This deliberately does
  not implement a general SQL parser or rewrite expressions. Unsupported output
  on another server must stop for review, not be ignored.
- Existing registered column/type/nullability/charset, FK endpoint/order/schema,
  index, migration checksum/history/journal and full DDL fingerprint checks remain
  unchanged. CHECK expressions remain covered by the unchanged complete DDL
  fingerprint; extracting names alone is not expression equivalence checking.
- No domain/application/GUI changes, new dependencies, privilege broadening,
  schema/migration edits or identity allowlist changes. Same MariaDB storage
  adapter architecture; this corrects read-only inspection compatibility, not
  an architectural or milestone scope expansion.

### Files created or modified in this correction

- Modified CMakeLists.txt: helper source and offline SchemaConstraintsTests target.
- Modified src/infrastructure/storage/MigrationRunner.cpp: SHOW CREATE name source.
- New src/infrastructure/storage/SchemaConstraints.h and SchemaConstraints.cpp.
- New tests/SchemaConstraintsTests.cpp: observed-style CommandRecord DDL, CHECK/FK
  names and composite FK, CRLF, missing/extra/renamed names, duplicate/malformed
  rejection, empty/unsupported input, escaped identifiers and no-CHECK table.
  Source-derived SHOW CREATE-style fixtures cover all nine CREATE definitions
  in embedded migrations (35 CREATE CHECKs / 12 CREATE FKs; these include historical
  definitions, not the cumulative schema-2 counts). No SQL connection is created.
- Updated docs/Building.md troubleshooting, current Roadmap status and this report.
  Earlier B7 proposal files/checksum manifest and historical reports are unchanged.

### Actual agent-executed validation

```bash
cmake --build build --parallel 2
ctest --test-dir build -R '^SchemaConstraintsTests$' --output-on-failure
ctest --test-dir build --output-on-failure \
   -E '^(MariaDbIntegrationTests|OrganizationIntegrationTests)$'
git diff --check
sha256sum migrations/mariadb/001_PersistentTaskFoundation.sql \
   migrations/mariadb/002_OrganizationalManagement.sql docs/milestones/Phase1.1.md
```

Build succeeded. Initial offline suite: 15 passed / 1 new test failed with
`Unsupported SHOW CREATE TABLE constraint formatting; operator review required.`
The fixture had passed wrapped migration-source expressions into an intentionally
one-line SHOW CREATE reader. Fixed **fixture rendering only** to join the source's
continuation lines; production parser remained fail-closed. Fixture inventory was
also checked against actual source (9 / 35 / 12) before final assertions.
Focused rerun passed 1/1; final offline suite passed **16/16**, exit 0, 6.65 seconds.
Both live integration executables were explicitly excluded, not run or relabeled
as current passes. Both live-test CMake options remain OFF in the existing build.
No live database connection, SQL, grants, backup/restore or reset was executed.

Protected SHA-256s remain unchanged: 001
`bb901d7580398893b5c04ee263f813b9d05873eb4bae08e0daf377faf73b0ea0`;
002 `7de311935843e4fc04cbca74d025d9f38986a4154d70bdd29b35a2820f87b736`;
accepted Phase1.1.md
`ffbf123937642a3478662d817a4f522377bb0ae6271fec51810e5b735bc34bde`.
Markdown local references/fences, changed-file whitespace and git diff --check
passed. No schema diagram regeneration required: schema SQL is unchanged.

### Remaining work / next approved operator step

The Linux binary has been rebuilt here. Rerun the same password-prompt
`./build/TaskCompassMigrate --verify --to-version 1` block from the 2026-11-16
"Linux metadata/grants reviewed" entry, using taskcompass_migrate and the explicit
local dev configuration. Expect schema 1 / dataset
21be9da7-b767-4578-b972-cc9a35122ddb / exit 0. This is still the approved read-only
B7.1 step, not --apply. Stop/report a failure; do not change privileges to make it
pass. Actual live success with the corrected binary is **pending**.

Windows must rebuild/run offline checks with the correction and finish B7.1 under
its own account/server; neither is verified by this Linux build or CRLF fixture.
Prior synthetic integration results remain historical and unchanged. Any repeat
live integration/narrow-role execution requires its matching explicit approval.
B7.2 capture and all grants/migrations/restore/GUI writes remain unauthorized;
request the next checkpoint only after B7.1 results are reviewed. No commit, push,
merge, tag, Phase 1.3 or acceptance claim.


## 2026-11-17 — verification invocation needs full temporary configuration

Owner ran `build/TaskCompassMigrate --verify --to-version 1` alone and reported:

```text
Explicit isolated database configuration and a valid port are required. See docs/Building.md.
```

This is configuration rejection before connection, not a new schema validation
result. The earlier environment/password exports existed only inside a subshell;
they do not survive its exit. Prior response showed the executable invocation but
should have repeated the complete standalone block. No credential should be put
on the command line or saved to address this. Use the full approved read-only
block from repository root:

```bash
(
   set +x
   export TASKCOMPASS_DB_HOST=127.0.0.1 TASKCOMPASS_DB_PORT=3306
   export TASKCOMPASS_DB_NAME=taskcompass_dev TASKCOMPASS_DB_USER=taskcompass_migrate
   export TASKCOMPASS_DB_ISOLATED=1
   IFS= read -r -s -p 'taskcompass_migrate password: ' TASKCOMPASS_DB_PASSWORD
   printf '\n'
   export TASKCOMPASS_DB_PASSWORD
   ./build/TaskCompassMigrate --verify --to-version 1
   verificationExitCode=$?
   unset TASKCOMPASS_DB_PASSWORD
   printf 'Verification exit code: %s\n' "$verificationExitCode"
   exit "$verificationExitCode"
)
```

Enter the existing migrator password locally. Expected schema 1 / dataset
21be9da7-b767-4578-b972-cc9a35122ddb / exit 0; stop/report any failure. Corrected
binary live verification remains pending. No new source or database operation in
this reporting step; B7.2 and later mutations remain unauthorized. Milestone-only
edit; Markdown fences and `git diff --check` passed. Build/tests not rerun for
this clarification. No commit/push/merge/tag.


## 2026-11-17 — Linux B7.1 read-only preflight passed

### Actual owner-reported verification

Owner reran the complete temporary-configuration/password-prompt block above,
using the rebuilt TaskCompassMigrate, taskcompass_migrate@127.0.0.1:3306 and
explicit taskcompass_dev configuration. Reported output:

```text
Schema 1 verified; dataset 21be9da7-b767-4578-b972-cc9a35122ddb
Verification exit code: 0
```

**Linux B7.1 passed (owner-reported).** Combined with the earlier root metadata/
grants report, this completes the Linux baseline check: known MariaDB 10.5.29
server, five InnoDB tables, one dataset, exact migration-1 checksum, current schema
shape and DDL fingerprint verified, and reviewed SELECT-only migrator/diagnostic
plus narrowly scoped runtime grants. The verified dataset matches the accepted
Phase 1.1 dataset. Reported baseline counts remain Task 4 (including one tombstone
and one completed task), CommandRecord 28 and LocalChange 28; this verification
is not a fresh all-value data export or a backup.

The SHOW CREATE compatibility correction now has an actual owner-reported live
read-only success on Linux; prior catalog failure, configuration error and offline
fixture failure remain preserved above. This does not establish Windows runtime
success with the correction or narrow migration-role write coverage. No source or
privilege workaround was needed beyond the documented source-only correction.

### Remaining gates and authority

Windows must rebuild/offline-test the corrected source and finish its approved
B7.1 metadata/grants/PUBLIC and pinned schema-1 verification. Keep observations,
dataset IDs and evidence separate per host. Next capture requires a separate
B7.2 approval identifying the platform/new private external directory and stopped
writers. Repeat fresh preflight if state changes; today's observations do not
allow later use of stale data/privilege assumptions.

No B7.2 backup/export, account/grant change, populated clone import, migration,
GUI write, restore, reset or cleanup is authorized or performed by this report.
Phase 1.2 remains unaccepted; no Phase 1.3 work. Agent changed milestone/Roadmap/
TestingPhase1.2 status only, executed no live SQL, and made no source/migration or
private artifact changes. Markdown local-reference/fence checks and git diff
--check passed. Build/CTest were not rerun for this reporting-only update; the
16/16 offline result above remains the actual source-correction test evidence.
No commit, push, merge or tag.


## 2026-11-17 — standalone Windows B7.1 commands supplied

Owner requested Windows commands. The following operator instructions are within
existing B7.1 approval, not execution evidence. Windows results remain pending.
Use the MSYS2 UCRT64 terminal from the Windows repository root with the latest
source (including SchemaConstraints helper/tests and B7 SQL review files) copied
through the owner's normal source-transfer workflow. No commit/push is performed
or required by these instructions. Close all TaskCompass clients/writers first;
keep MariaDB running. Stop after any failed command or unexpected schema/grant.

### 1 — rebuild and offline checks

```bash
scripts/phase12/01-build.sh
sha256sum --check scripts/phase12/b7/ReviewChecksums.sha256
```

Run the checksum command only after build/tests succeed. Expected 16 passing
offline tests and 2 skipped live integration tests with the current sources; no
live integration reset is requested. The build script explicitly disables both
live-test registrations. All review checksums must match. These operations do not
provision/migrate/connect to a live database. They are pending owner execution on
Windows; the Linux results are not Windows evidence.

### 2 — metadata, existing grants and PUBLIC (root database password)

```bash
(
   set -o pipefail
   cat scripts/phase12/b7/preflight-dev.sql \
       scripts/phase12/b7/preflight-dev-grants.sql \
       scripts/phase12/b7/public-grants.sql |
      env -u MYSQL_PWD '/c/Program Files/MariaDB 13.0/bin/mariadb.exe' \
         --no-defaults --protocol=TCP --host=127.0.0.1 --port=3306 \
         --user=root --password --default-character-set=utf8mb4 \
         --batch --raw --database=taskcompass_dev
   preflightExitCode=$?
   printf 'Windows preflight exit code: %s\n' "$preflightExitCode"
   exit "$preflightExitCode"
)
```

Enter MariaDB root password locally (not Windows login). The known native client
path must match the actual trusted installation; stop if absent. Expect
feldria-w10 / 13.0.2-MariaDB / port3306 / taskcompass_dev, schema1 and its five
InnoDB tables, reviewed unchanged Phase1.1 grants and no PUBLIC write/DDL authority
covering TaskCompass targets. Ignore only unrelated PUBLIC test/test_% grants as
specified in the review package. Do not infer Linux counts/UUID apply to Windows.
Share the nonsecret report and stop for review if anything is unexpected. No grant
or schema changes, account credentials or task payloads are selected.

### 3 — pinned schema verification (taskcompass_migrate password)

```bash
(
   set +x
   export MSYS2_ENV_CONV_EXCL="${MSYS2_ENV_CONV_EXCL:+$MSYS2_ENV_CONV_EXCL;}TASKCOMPASS_DB_PASSWORD"
   export TASKCOMPASS_DB_HOST=127.0.0.1 TASKCOMPASS_DB_PORT=3306
   export TASKCOMPASS_DB_NAME=taskcompass_dev TASKCOMPASS_DB_USER=taskcompass_migrate
   export TASKCOMPASS_DB_ISOLATED=1
   IFS= read -r -s -p 'taskcompass_migrate password: ' TASKCOMPASS_DB_PASSWORD
   printf '\n'
   export TASKCOMPASS_DB_PASSWORD
   ./build-ucrt64/TaskCompassMigrate.exe --verify --to-version 1
   verificationExitCode=$?
   unset TASKCOMPASS_DB_PASSWORD
   printf 'Windows verification exit code: %s\n' "$verificationExitCode"
   exit "$verificationExitCode"
)
```

Run the complete block after successful metadata review, not its final executable
line alone. Password is the existing migrator account's, not root/diagnostic.
Expected schema1 / same Windows dataset as step2 / exit0. No --apply or migration.
B7.2 captures/grants/DDL/restores remain separately gated. Preserve all artifacts.

### Reporting validation

Milestone-only command documentation. All three groups' Bash syntax checked using
`bash -n` without executing them; Markdown local links/fences and git diff --check
passed. No build/CTest, live SQL, account change, backup/restore or Windows command
was executed by the agent for this documentation step. No commit/push/merge/tag.


## 2026-11-17 — Windows corrected build and B7.1 metadata/grants passed

### Actual owner-reported commands and results

Owner ran the standalone Windows sequence documented above in UCRT64 on
feldria-w10 from C:/Users/justin/repo/TaskCompass:

```bash
scripts/phase12/01-build.sh
sha256sum --check scripts/phase12/b7/ReviewChecksums.sha256
```

CMake configured/generated, the corrected SchemaConstraintsTests.exe linked,
and CTest reported **16 passed, 2 skipped, 0 failed** out of 18 registered tests,
70.85 seconds. SchemaConstraintsTests passed (0.07 seconds); the two live suites
MariaDbIntegrationTests and OrganizationIntegrationTests were skipped as intended.
This is Windows compilation/offline regression evidence for the compatibility
correction, not a new live migration/integration run. All 20 review checksum entries
(runbook and 19 proposed SQL files) reported OK.

Owner then ran the exact root TCP metadata/grants/PUBLIC pipeline above. It
completed with `Windows preflight exit code: 0` and supplied the following:

| Observation | Actual owner-reported value / review |
| --- | --- |
| Server / database / identity | feldria-w10, 13.0.2-MariaDB, port 3306, taskcompass_dev, root@localhost |
| Server settings | lower_case_table_names=1; FK and CHECK enforcement both 1 |
| Encoding | utf8mb4 / utf8mb4_unicode_ci |
| Objects | Exactly five BASE TABLE/InnoDB objects, lowercased on Windows: commandrecord, dataset, localchange, schemamigration, task |
| Stored code / other target sessions | No routine, trigger, event or other selected-target session rows displayed |
| Migration | Exactly version 1, checksum bb901d7580398893b5c04ee263f813b9d05873eb4bae08e0daf377faf73b0ea0; appliedAt 1794691232396 |
| Stored fingerprint | 6b3bd320ea92630b21909de0bd7cd38ea3bc3f644a69ba1fd249db43e656a343; current fingerprint still requires Windows Qt verification |
| Dataset | 457bb52b-fea4-4347-9c47-675c6b035c90; createdAt 1794691232396; count 1 |
| Counts | SchemaMigration 1, Dataset 1, Task 2, CommandRecord 16, LocalChange 16 |
| Task states | Both active and not tombstoned; no other lifecycle/tombstone groups displayed |
| Scoped accounts | Diagnostic, migrator and app each at 127.0.0.1 only |
| Direct grants | Each global USAGE only, not grantable; diagnostic/migrator each exact escaped taskcompass_dev schema SELECT; app exactly 9 approved table rights, all not grantable |
| Extra columns / roles | No column-privilege or explicit-role rows displayed |
| PUBLIC | Only database scopes `test` and `test\_%`; no TaskCompass/global/role grant displayed |

PUBLIC's substantial DML/DDL rights are confined to unrelated test scopes. The
escaped underscore in test\_% is literal; neither scope matches taskcompass_dev,
taskcompass_phase12_test or proposed taskcompass_phase12_restore. These are not
TaskCompass privilege failures and must not be changed. No privileged test-schema
access was attempted. The supplied SHOW CREATE output matches expected schema-1
structure on inspection; case-normalized Windows table names are expected.

### Status and next approved check

**Windows build/checksums and B7.1 metadata/grants passed (owner-reported).** Windows
B7.1 is not fully passed until the corrected Qt verifier runs as the existing
SELECT-only taskcompass_migrate account. Run the complete Windows password-prompt
block from the preceding standalone command entry, including MSYS2_ENV_CONV_EXCL,
explicit local dev environment and:

```bash
./build-ucrt64/TaskCompassMigrate.exe --verify --to-version 1
```

Do not run that line alone without its temporary environment. Expect schema 1 /
dataset 457bb52b-fea4-4347-9c47-675c6b035c90 / exit 0. Stop on failure; no --apply,
new grants, repair or backup. Linux B7.1 remains passed; keep platform counts and
UUIDs separate. B7.2 and subsequent live stages still need explicit approval.

### Reporting changes and agent validation

Updated this milestone, Roadmap and TestingPhase1.2 current status, preserving
prior results/failures. No source, migrations, SQL review artifacts or private
evidence changed. Markdown local-reference/fence checks and git diff --check
passed. No agent build/CTest or live SQL was executed for this reporting-only
step; all Windows results above are owner-reported. No backup, restore, account/
grant change, reset, Phase 1.3 or commit/push/merge/tag.


## 2026-11-17 — Windows Qt connection-open failure

Owner ran the complete Windows temporary-environment verification block. Output:

```text
Cannot open MariaDB. Check service/TCP availability, QMYSQL, account, password and grants.
Windows verification exit code: 1
```

This is **connection-open failure before schema inspection**, not a CHECK/fingerprint
failure or evidence of an interrupted migration. Source review of
DatabaseConnection.cpp confirms this message is emitted only when QSqlDatabase::open
fails; session initialization and identity/schema queries have not succeeded.
The connection has a 5-second connect/read/write timeout and suppresses raw connector
error text to avoid disclosing connection data/credentials. No cause is established.
Wrong migrator password, availability/network or Qt/client loading remain possible;
the earlier successful root TCP connection does not validate this account's password.
No reset, password change, grant broadening or schema operation is appropriate yet.

### Next read-only account check on Windows UCRT64

Use the native MariaDB client and the same explicit TCP database/account to obtain
a useful authentication/network error independently of Qt. Enter the existing
**taskcompass_migrate** password (not root, diagnostic or Windows login):

```bash
env -u MYSQL_PWD '/c/Program Files/MariaDB 13.0/bin/mariadb.exe' \
   --no-defaults --protocol=TCP --host=127.0.0.1 --port=3306 \
   --user=taskcompass_migrate --password --database=taskcompass_dev \
   --batch --execute='SELECT VERSION(),DATABASE(),CURRENT_USER();'
```

If this succeeds, expect 13.0.2-MariaDB, taskcompass_dev,
taskcompass_migrate@127.0.0.1. Rerun the **complete** Qt verification block with a
fresh hidden password entry; all other configuration remains unchanged. If that
still fails, run the database-free Qt plugin listing for the next report:

```bash
./build-ucrt64/TaskCompassDbDiagnostic.exe
```

The diagnostic above has no --connect: it checks drivers only, not login or schema.
If native login fails, stop and report the nonsecret client error instead of
rerunning setup. Never print environment/password contents or paste credentials.
No live operation was executed by the agent; these commands await operator results.

### Status, changes and validation

Windows build/offline checks, review checksum and root metadata/grant results remain
passed; Windows B7.1 is **incomplete**. Linux B7.1 remains passed. Updated milestone
and Roadmap current status, preserving all earlier evidence. Markdown references/
fences and git diff --check passed. No source/migration/SQL review package/private
artifact changes, no agent build/CTest/live SQL and no account/grant/backup/restore
operation. B7.2 and all later live gates remain unauthorized. No commit, push,
merge, tag or Phase 1.3 work.


## 2026-11-17 — Windows verification passed; B7.1 complete on both platforms

### Owner-reported result and diagnosis

Owner identified that Windows uses a different migrator password from Linux and
reran the complete approved Windows verification block with the Windows password:

```text
Schema 1 verified; dataset 457bb52b-fea4-4347-9c47-675c6b035c90
Windows verification exit code: 0
```

**Windows B7.1 passed (owner-reported).** The prior connection-open failure is
explained by the owner's password mix-up, followed by successful login/verification;
no password reset, grant change or Qt connection workaround was needed. Do not
record a native-client login or extra plugin diagnostic as executed: neither was
reported. No credentials were shared or stored in this record.

| B7.1 observation | Linux | Windows |
| --- | --- | --- |
| Server | feledria / MariaDB 10.5.29 | feldria-w10 / MariaDB 13.0.2 |
| Approved local TCP target | taskcompass_dev, 127.0.0.1:3306 | taskcompass_dev, 127.0.0.1:3306 |
| Verified schema / exit | 1 / 0 | 1 / 0 |
| Verified dataset UUID | 21be9da7-b767-4578-b972-cc9a35122ddb | 457bb52b-fea4-4347-9c47-675c6b035c90 |
| Metadata baseline counts | Task 4, CommandRecord 28, LocalChange 28 | Task 2, CommandRecord 16, LocalChange 16 |
| Grants | Reviewed exact schema-1 rights | Reviewed exact schema-1 rights; unrelated PUBLIC test grants accepted |
| Evidence source | Owner-reported live results; agent-executed Linux build/offline checks | Owner-reported build/offline/checksum and live results |

The corrected verifier now has owner-reported read-only success on both supported
servers with their existing least-privilege migrator accounts. Earlier errors,
reports, SQL and all Phase 1.1 evidence remain preserved. Both B7.1 gates are met;
this is not approval to back up, migrate, change privileges or write GUI data.
Baseline counts are prior metadata observations, not new all-value snapshot exports.

### Approval Requested — B7.2 fresh populated schema-1 backups and private exports

Request separate approval for **B7.2 only on both above TaskCompass development
databases**, following the exact [B7.2 operator procedure](../Phase1.2B7.md#b72--fresh-populated-schema-1-backup-and-evidence)
and common definitions in the reviewed runbook. No schema/account/grant changes.
Proposed **new external directories**, independently timestamped per platform:

```text
Linux:   $HOME/temp/TaskCompassBackups/Phase1.2-schema1-linux-<YYYYMMDD-HHMMSS>
Windows: $HOME/TaskCompassBackups/Phase1.2-schema1-windows-<YYYYMMDD-HHMMSS>
```

Approval binds these fresh directory patterns and known hosts/databases. Choose
a new path at execution, record its concrete value, reject an existing destination,
and preserve every old backup/export/restored integration database. Stop all
TaskCompass/SQL writers, repeat pinned schema-1 verification and confirm unchanged
baseline/privileges before capture; any drift requires review before proceeding.
Do not stop/reconfigure the MariaDB service.

Exact approved-procedure command proposed (not yet executed):

```bash
scripts/phase11/09-backup.sh --backup-dir "$b7Schema1Directory" \
   --client "$taskCompassClient" --dump "$taskCompassDump" --build-dir "$taskCompassBuild"
```

The runbook common definitions supply platform-specific trusted native client/dump
paths and build directories. This existing helper is limited to the five-table
schema-1 capture here. It verifies with taskcompass_app and dumps/exports with
SELECT-only taskcompass_dev, not root; their **platform-specific** passwords are
entered at local hidden prompts. No credential file is added. New directory
privacy is checked before files (umask077 on Linux, protected owner ACL on Windows).
If privacy cannot be established, stop without capturing data.

Included scope: private after-gui.sql with exit/nonempty/hash checks, existing
source.tsv/source-after.tsv/hash/server/dataset evidence, and the runbook's new
38-legacy-column canonical before/after exports plus schema1 SHOW CREATE export
and checksum manifest. These filenames identify a new pre-upgrade baseline; they
do not overwrite Phase 1.1 artifacts. Require exact same-platform before/after
rows and matching fresh dataset/schema checks. Canonical HEX remains private data.
Owner privately reviews the full dump for only expected table DDL/INSERTs and
normal session prologue/epilogue before its hash can be proposed for later restore.
No exports/dumps are to be committed or pasted into conversation.

Expected: exit0, nonempty fresh dump, all recorded hashes verified, no source drift,
all five tables/all legacy columns/history/tombstones preserved, correct UUID and
encoding, no unexpected dump SQL. No restore is included, so capture alone is
**not** proof of recovery. Failure: preserve failed files/private evidence, stop;
no overwrite, retry into the directory, cleanup or automatic repair. A replacement
capture uses a fresh reviewed directory. B7.3 must later bind the actual privately
reviewed dump hash and exact empty isolated target before any import.

Excluded: all restores/provisioning/grant changes/migrations, integration resets,
GUI writes, source database writes, modifying old artifact ACLs/content, deleting
files or databases, Phase 1.3 and commit/push/merge/tag. This request is the explicit
checkpoint required by the owner's B7 database-safety instructions; earlier B7.1
approval does not authorize it. Await owner decision before any capture/export.

### Reporting changes and agent validation

Updated this milestone, Roadmap and TestingPhase1.2 status. Markdown local-reference/
fence checks and git diff --check passed. No source/migration/SQL review package or
private evidence changes; no agent live SQL, backup/export, restore, privilege or
account operation. Build/CTest not rerun for this reporting-only update; the actual
previous platform results are preserved above. Phase 1.2 remains unaccepted.


## 2026-11-17 — B7.2 approved; operator-only commands prepared

### Explicit owner decision

Owner approved B7.2 on Linux and Windows, limited to the reviewed fresh schema-1
backup/private evidence capture, with independently timestamped destinations,
stopped writers, fresh verification and unchanged baseline/grants, private
permissions, complete exports/comparisons/checksums. Owner explicitly instructed
**do not execute on my behalf**, stop/report unexpected conditions, no retries/
repair, and excluded migrations, restores, provisioning, grants, resets, GUI writes,
cleanup, commit or push. This supersedes the preceding B7.2 approval request only;
no later stage is authorized. All Phase1.1 artifacts/databases remain protected.

### Exact operator commands and expected results

Created [Phase1.2B72.md](../Phase1.2B72.md), a self-contained copy/paste companion
to the unchanged, checksum-pinned B7.2 review procedure. Its four sections contain:

1. Exact platform-specific local client/dump/build paths, MSYS2 password environment
   conversion exclusion, known dataset UUID and new directory assignment:
   Linux `$HOME/temp/TaskCompassBackups/Phase1.2-schema1-linux-<timestamp>`;
   Windows `$HOME/TaskCompassBackups/Phase1.2-schema1-windows-<timestamp>`.
   File/tool/version/checksum checks before capture; assignment creates no folder.
2. Fresh reviewed root SELECT/SHOW baseline/grants per host, including Windows
   PUBLIC. Require unchanged known server/version/encoding/schema/dataset/counts/
   migration checksum/fingerprint/grants; no stored code or other target writers.
   Manual review is a real stop boundary, not an automatic assumption of success.
3. Complete shared fail-fast capture block: fresh explicit migrator verify1,
   existing Phase1.1 09-backup helper with schema1 only, expected UUID match,
   private directory permissions/Windows owner ACL established by helper,
   nonempty dump/hash and source/source-after equality, all 38 original column
   exports before/after comparison, raw schema1 DDL, second pinned verify1 and
   checksums. Native client/dump version files are written only after private
   directory success; included with other fresh private metadata in checksum seal.
   Export destinations and checksum manifests refuse overwrite. All failures stop;
   no cleanup, repair or retry. Credential prompts remain local/session-only.
4. Exact `less` command for complete private SQL review and required nonsecret
   result summary. No restore/import command is included. Stop after capture/review.

Expected per platform: exit0, both verify1 calls and helper runtime verification
show its known UUID, helper reports populated consistent source capture, checksum
entries all OK, matching five-table source exports and canonical 38-column exports,
and private dump review passes. Linux baseline Task/CommandRecord/LocalChange
4/28/28; Windows 2/16/16. Prior counts are fresh-preflight comparison criteria, not
newly observed data. A changed baseline must stop for review. Quiescence is required
because separate captures are not one transaction. A successful backup is not a
successful restore/recovery test. Actual generated directory/hash/version values
must come from owner output later, not assumptions or fabricated examples.

Passwords: fresh/final verify uses each platform's existing taskcompass_migrate;
helper first verifies with taskcompass_app, then dumps/exports using SELECT-only
taskcompass_dev. Three additional native exports prompt for diagnostic password
individually, consistent with the reviewed mechanism. Linux root metadata uses
sudo/OS password; Windows metadata uses its MariaDB root password. No account,
authentication, password or grant change; no persistent credential file.

### Changes and actual agent validation

New docs/Phase1.2B72.md; updated this milestone, Roadmap and TestingPhase1.2 status/
links. No executable/SQL/app/migration/diagram or existing backup was changed.
Agent checked the guide's Bash code fences with `bash -n` through Python subprocess
stdin (**7 blocks**, syntax only), checked Markdown links/fences/whitespace and ran:

```bash
sha256sum --check scripts/phase12/b7/ReviewChecksums.sha256
git diff --check
```

All 20 reviewed package checksum entries passed; local-reference/fence/whitespace
checks and git diff --check passed. No operator command was executed by the agent:
no database connections/SQL, directories, dumps, exports, backups/restores,
provisioning, grants, reset or GUI write. No build/CTest was needed or run for
this documentation-only step; Windows command/ACL/capture execution is untested
here. Previous build and live B7.1 evidence remains historical and unchanged.
No commit, push, merge, tag or Phase1.3 work.

### Remaining work

Await separate owner-reported B7.2 results for each platform: fresh baseline/grants,
native versions, concrete private directory, capture exit/PASS, checksum results,
exact after-gui.sql hash, UUID/count summary and private SQL review outcome. Stop
on any failure; preserve all artifacts. B7.3 needs a new explicit approval binding
the actual reviewed dump hash and exact isolated target before import. This report
requests no additional permission for the already approved B7.2 operator procedure.


## 2026-11-17 — numbered B7.2 scripts replace manual command pasting

### Owner request, scope and implementation

Owner requested a test script for each step because the manual blocks were too
long. Implemented operator-only wrappers within the already approved B7.2 capture
scope. **No live procedure was executed by the agent.** Existing authorization
remains limited to fresh schema-1 backup/private evidence on the two known hosts;
no source database write, schema/grant/account change, import/reset/repair/cleanup
or Phase1.3 work. The checksum-pinned review package and SQL remain unchanged.

Same four commands on Linux Bash and Windows UCRT64, from repository root:

```bash
scripts/phase12/b72/01-check.sh
scripts/phase12/b72/02-preflight.sh --approved
scripts/phase12/b72/03-capture.sh --approved
scripts/phase12/b72/04-review.sh
```

Run **one command at a time**. Step2 is a read-only report requiring manual review
against B7.1, not an automatic grant/baseline certification. Step3 refuses to begin
until the operator types `BASELINE UNCHANGED; WRITERS CLOSED`, then uses the existing
helper's separate `ALL TASKCOMPASS WRITERS CLOSED` confirmation. Do not proceed if
any baseline/grant changes or any writer is active. No preflight receipt/session
configuration is persisted or blindly sourced between scripts.

Step1 checks reviewed checksums, native tools/versions, built verifier and review/
ACL dependencies with no connection or backup. Step2 uses Linux sudo/socket or
Windows native password-prompt TCP root to execute only the same reviewed metadata/
grant/PUBLIC files, prints expected host/UUID/count/right summary and leaves review
to the operator. Step3 freshly verifies1 with explicit fixed local dev config and
known platform UUID, chooses a new timestamped external destination automatically,
delegates to the existing Phase1.1 private schema-1 backup helper, checks dataset,
nonempty dump and five-table comparison, writes 38-column canonical before/after
exports plus raw DDL/native versions, re-verifies1 and seals/checks every artifact.
Additional diagnostic prompts identify the current platform's taskcompass_dev.
New files never overwrite prior files; all errors stop and preserve evidence.

Step4 prompts for the plain directory printed by step3 (or optional --backup-dir),
requires outside-repository private directory and known nonlinked files, verifies
fixed manifest coverage/hashes before reading evidence, checks UUID/comparisons,
opens the dump in less with pager history disabled, and requires `DUMP REVIEWED`.
It runs no SQL and changes no artifact/ACL. Manifest entries cannot redirect reads
to unexpected paths; Linux owner/mode and Windows protected owner ACL are checked.
Private review is an owner assertion, not an automatic SQL safety parser. Any
forbidden/unexpected SQL stops; no edit/retry/restore is authorized.

Credentials remain per-platform local prompts/session memory, never saved or
printed; root password is never captured by the new scripts. Qt errors are redacted
against the prompted migrator secret. No dependencies/configuration/accounts or
new architectural abstractions. Tests clean up only their own temporary fake
fixtures, not any TaskCompass backup/database artifact.

### Files created or modified

- New scripts/phase12/b72/01-check.sh, 02-preflight.sh, 03-capture.sh, 04-review.sh;
  executable numbered wrappers with automatic platform selection.
- New scripts/phase12/b72/run.sh: shared B7.2-only implementation, --help/--print-plan,
  explicit live-report/capture approval gates and optional trusted tool/path overrides.
  No action for apply/import/reset/provision/grant or arbitrary target credentials.
- New tests/Phase12BackupScripts.test.sh; CMakeLists.txt registers Phase12BackupScripts
  with the existing Linux/Windows shell-test timeout policy.
- Updated docs/Phase1.2B72.md to concise numbered-script instructions, account prompt
  meanings, expected baselines/results and explicit manual review/stop conditions.
  Underlying manual procedure remains in unchanged docs/Phase1.2B7.md; previous
  approval/design/result entries above are preserved.
- Updated current pointers in this milestone, Roadmap and TestingPhase1.2. No
  production application source, migrations, diagrams, existing helper scripts,
  proposed SQL or protected Phase1.1 evidence changed in this step.

### Actual offline tests and results

```bash
bash -n scripts/phase12/b72/run.sh
bash tests/Phase12BackupScripts.test.sh
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure \
   -E '^(MariaDbIntegrationTests|OrganizationIntegrationTests)$'
ctest --test-dir build --output-on-failure -R '^Phase12BackupScripts$'
sha256sum --check scripts/phase12/b7/ReviewChecksums.sha256
git diff --check
```

Build/reconfiguration succeeded. Full offline suite **17/17 passed**, exit0,
7.11 seconds, with both live suites explicitly excluded (not executed). Both
live-test registrations remain OFF. Focused CTest rerun passed 1/1 (0.43 seconds);
final direct Bash harness rerun after prompt/permission-fixture refinements passed
all five PASS groups. Tests use fake client, dump, verifier, sudo, pager, path and
PowerShell executables; no real server connection, dump or live SQL. Linux private
fixture mode is also checked using actual stat; mocked Linux permission metadata
allows the same negative-visibility tests to run on a Windows harness. Windows
ACL/path coverage here is mocked, not native Windows execution evidence.

Initial harness stopped after its third PASS group: the drift scenario reused an
older fixture marker so both exports changed identically. Corrected the **mock**
to a scenario-specific marker; mismatched export now correctly fails capture with
artifacts preserved. No retry/repair of a real capture occurred or was needed.

Covered previews/no-approval/no-confirmation with zero connection calls; wrong
schema/dataset or failed verifier before directory creation; secret redaction;
actual existing-helper delegation to fake tools; spaces in paths; directory
privacy rejection; successful full comparison/hash/review chain; no overwrite;
export drift, tampered evidence and failed dump cannot seal/review; mocked Windows
root TCP versus Linux socket routing; ACL failure before files; interactive
review-directory input; and rejected manifest path traversal. Fixed target/config
assertions and fake-executable allowlists reject any migration apply or SQL mutation.

All 20 existing review checksums, new shell syntax/whitespace, Markdown links/fences
and git diff --check passed. No renderer/diagram regeneration or dependency install.
No real B7.2 directory, backup/export, database/account/grant/reset/restore or GUI
operation was performed by the agent. No commit, push, merge or tag.

### Remaining validation and reporting

Owner must run the four stages separately on each platform and report nonsecret
results/path/hash and private review outcome. The new scripts themselves have not
been live-validated on Linux or Windows; prior manual B7.1 and synthetic migration
results do not imply capture success. Windows offline suite with this new script
target is also pending owner execution (expected 17 offline passes / 2 live skips).
No further approval is required for these already approved B7.2 operator actions.
Any unexpected condition stops for review. B7.3 still requires separate exact-target/
reviewed-artifact-hash approval; no restore or later operation follows automatically.


## 2026-11-17 — Linux B7.2 capture stopped at final verification

### Owner-reported progress and failure (preserved evidence)

Owner ran steps 01, 02 and 03 on Linux. Step01 passed all reviewed checksums and
reported native MariaDB client 15.1 / Distrib10.5.29 and mariadb-dump10.19 /
Distrib10.5.29, Linux x86_64. Step02 reported the unchanged approved baseline:
feledria/10.5.29/3306, taskcompass_dev, five InnoDB tables, known checksum/fingerprint,
Dataset21be9da7-b767-4578-b972-cc9a35122ddb, Task4, CommandRecord28, LocalChange28,
expected lifecycle/tombstone breakdown and exact unchanged grants with no extra
role/column/stored-code/session output. These are owner observations, not new
agent database queries.

Step03 confirmations and initial taskcompass_migrate verify1 succeeded; helper
runtime taskcompass_app verify1 also succeeded. Fresh destination:

```text
/home/justin/temp/TaskCompassBackups/Phase1.2-schema1-linux-20261117-013703
```

Helper reported dump exit0/nonempty/hash recorded and consistent source exports;
`after-gui.sql: OK` and `source.tsv: OK` printed. Three additional exports completed
to legacy-before.hex.tsv, schema1.ddl.txt and legacy-after.hex.tsv. Reaching final
verification means their comparison passed under the fail-fast script; the agent
has not independently inspected those private files. Final migrator prompt failed:

```text
Cannot open MariaDB. Check service/TCP availability, QMYSQL, account, password and grants.
STOP: Read-only schema-1 verification failed. Preserve evidence; no retry, cleanup or repair.
```

Owner reports a password typo at that final prompt. Raw connector cause was not
independently tested. No final capture exit code or dump digest was supplied in
the transcript. **B7.2 Linux is incomplete**, not accepted or a restore-ready
artifact. Based on the code path, client-version.txt, dump-version.txt and final
b7-schema1-evidence.sha256 were not reached and are expected absent; completion
must inspect/refuse any different state rather than assume this is true.

Do not repeat step03 (which would create another backup) or run step04 against
this unsealed directory. Keep all existing dumps/exports/metadata, including old
Phase1.1 evidence, unchanged. No existing-directory read/backup/SQL was executed by
the agent; source preparation/offline mocks only follow.

### Prepared source-only targeted completion

New scripts/phase12/b72/03a-finish-capture.sh invokes a separately gated `finish`
action in run.sh; it is never called automatically. This adds no migration recovery
path and is not a general partial-capture repair. The actual reviewed case is a
final read-only verification failure after completed dump/export comparisons.
Missing evidence, failed original hash/comparison, extra completion metadata,
wrong UUID/current data or unsuccessful verify1 stops without sealing.

The operation requires --approved, explicit existing --backup-dir and confirmation
`FINISH REVIEWED CAPTURE; WRITERS CLOSED`. It rechecks private directory permissions,
regular nonlinked files, original helper hashes, source and legacy comparisons,
known dataset, and absence of all three completion metadata files. It reads current
legacy columns through SELECT-only taskcompass_dev into SHA-256 directly (no new
export file or printed task payload), compares to the captured canonical file,
then invokes pinned verify1 using taskcompass_migrate. Only after those pass does
it write new native version files and final checksum manifest with noclobber,
reusing the existing sealing helper and reporting original dump hash. It never
redumps/re-exports, deletes, overwrites, changes ACLs or writes to the database.
The three metadata files can still be partially created if a version/seal command
fails; that new state requires review and this action refuses it on another call.

Modified run.sh factors existing directory/privacy and final sealing code into
shared functions; normal capture/review remain the same. New wrapper, expanded
Phase12BackupScripts.test.sh and guide exception/reporting are the only changes
besides milestone/current-status documentation. No application/C++/SQL/migration
or grant changes. Source preparation requires no live execution authorization.

### Offline validation and limitations

```bash
bash -n scripts/phase12/b72/run.sh
bash tests/Phase12BackupScripts.test.sh
ctest --test-dir build --output-on-failure \
   -E '^(MariaDbIntegrationTests|OrganizationIntegrationTests)$'
sha256sum --check scripts/phase12/b7/ReviewChecksums.sha256
git diff --check
```

Expanded fake-executable harness passed six PASS groups. Added explicit reproduction
of failure at the third (final) verifier invocation, no-approval/confirmation
refusal, current-data drift refusal, failed verify with no seal/version writes,
successful manually invoked completion with original dump unchanged, and reseal
refusal. Existing capture/review guards and mocked Windows ACL flow still passed.
Full Linux offline suite passed 17/17; both live suites excluded and not executed.
All reviewed checksums, shell syntax and Markdown local-reference/fence/whitespace
checks passed. Shell-only change; no application build was necessary or rerun.
Actual Linux completion and Windows native completion remain untested/pending.
No real capture artifact or live SQL was accessed by the agent. No commit/push/
merge/tag or Phase1.3 work.

### Approval Requested — one-time completion of the retained Linux capture

Request **operator-only** approval for this exact Linux target/path and failure
state, after stopped writers and a repeated step02 baseline/grant report still
match. This request is separate because the approved procedure explicitly stopped
on unexpected failure and prohibits automatic retry/repair. Do not execute until
owner authorizes this concrete limited completion:

```bash
scripts/phase12/b72/02-preflight.sh --approved
```

Review unchanged baseline/grants first, then:

```bash
scripts/phase12/b72/03a-finish-capture.sh --approved \
   --backup-dir /home/justin/temp/TaskCompassBackups/Phase1.2-schema1-linux-20261117-013703
```

This requests only existing artifact integrity checks, current SELECT-only legacy
hash comparison, final explicit `--verify --to-version 1`, and creation of the three
missing version/checksum metadata files. Passwords are existing Linux diagnostic
then migrator accounts, entered privately; no root credential is used for sealing.
No backup/export replacement, retries, database writes/migration/restore/account/
grant change or cleanup. Original dump digest remains to be reported; no restore
approval or digest is fabricated. Stop and preserve any mismatch/partial file state.

Expected: original capture validated, current data unchanged, schema1/same UUID,
all checksum entries OK, final `PASS: reviewed B7.2 completion` and original dump
SHA-256 printed. Then the existing step04 can perform private SQL review:

```bash
scripts/phase12/b72/04-review.sh \
   --backup-dir /home/justin/temp/TaskCompassBackups/Phase1.2-schema1-linux-20261117-013703
```

This is private file review only; operator must inspect all SQL before confirming.
Completion/review outcomes must be reported before B7.2 Linux is marked passed.
Windows B7.2 and B7.3/later stages remain separately pending; no import/upgrade is
included. Await owner approval; no operation is executed on the owner's behalf.


## 2026-11-17 — targeted Linux capture completion approved

Owner first invoked `03a-finish-capture.sh` without arguments and reported:

```text
STOP: Use --approved only under the recorded B7.2 approval. Preserve evidence; no retry, cleanup or repair.
```

That guard stops before any connection or artifact operation; it is not a new
capture/authentication failure. Owner then explicitly stated **"I approve the
targeted completion"**. This approves the exact preceding operator-only request
for the retained Linux directory below, including fresh unchanged-baseline review,
read-only current-data/hash and schema1 verification, and creation of only the
missing version/checksum metadata. Earlier request and failure history remain
preserved. No expanded targets, redump/re-export, overwrite, database mutation,
restore, migration, provisioning, privilege change, reset or cleanup is authorized.
Agent must not execute the procedure; owner execution/results remain pending.

### Exact approved operator commands

Keep all writers stopped and run one command at a time from repository root:

```bash
scripts/phase12/b72/02-preflight.sh --approved
```

Review the unchanged Linux baseline/grants before continuing. Then:

```bash
scripts/phase12/b72/03a-finish-capture.sh --approved \
   --backup-dir /home/justin/temp/TaskCompassBackups/Phase1.2-schema1-linux-20261117-013703
```

Confirmation: `FINISH REVIEWED CAPTURE; WRITERS CLOSED`. Passwords in order: Linux
**taskcompass_dev** for current-data hash (no export file), then Linux
**taskcompass_migrate** for final pinned verification. Expect schema1 /
21be9da7-b767-4578-b972-cc9a35122ddb, checksum OK entries, printed original dump
hash and `PASS: reviewed B7.2 completion`. No capture file is replaced.

Only after completion succeeds, run:

```bash
scripts/phase12/b72/04-review.sh \
   --backup-dir /home/justin/temp/TaskCompassBackups/Phase1.2-schema1-linux-20261117-013703
```

Privately inspect the entire dump in less, quit with q, and type `DUMP REVIEWED`
only if all SQL passes the documented checklist. Report completion/review output
and SHA-256 without private exports or secrets. Stop on any error/discrepancy and
preserve state; no further retry or repair is implied by this one-time approval.
Windows B7.2 and B7.3+ remain pending; no import/upgrade permission follows.

### Reporting validation

Updated milestone, operator guide/current Roadmap/TestingPhase1.2 approval pointers.
Markdown references/fences and git diff --check passed. No source, SQL, migration,
private artifact or credential changed; no build/CTest rerun for status-only docs.
No agent live SQL, capture/completion/backup/restore or directory access. No commit,
push, merge, tag or Phase1.3 work. B7.2 remains incomplete until actual owner results.


## 2026-11-17 — Linux B7.2 completed and privately reviewed

### Owner-reported fresh baseline and targeted completion

Owner repeated `02-preflight.sh --approved`: after one rejected sudo OS-password
entry, sudo authenticated and the read-only report completed. Baseline remains
feledria/10.5.29/3306, taskcompass_dev, five InnoDB tables, known migration1 checksum/
fingerprint and dataset, Task4/CommandRecord28/LocalChange28 with the unchanged
lifecycle/tombstone counts and exact reviewed schema1 account rights. No unexpected
object, role/column privilege or session row was shown. The OS-password retry
occurred before SQL execution, not as a database repair.

First targeted completion invocation entered `FINISHED REVIEWED CAPTURE; WRITERS
CLOSED` instead of `FINISH ...`; confirmation guard rejected it before capture
inspection, SQL connection or file mutation. Owner then reran with the exact
confirmation under the already approved scope; no agent automatic retry occurred.
The completed operation reported:

```text
Schema 1 verified; dataset 21be9da7-b767-4578-b972-cc9a35122ddb
PASS: reviewed B7.2 completion; original capture files unchanged.
```

Both original helper checksums and all final evidence entries reported OK:
after-gui.sql, source.tsv, legacy-before.hex.tsv, legacy-after.hex.tsv,
schema1.ddl.txt, source-after.tsv, dataset.txt, server.txt, client-version.txt and
dump-version.txt. Successful completion also indicates its required private-
directory/file checks, before/after comparisons and current read-only legacy-data
hash comparison passed. Those comparisons are script-reported, not independent
agent inspection of private data. No separate numeric exit status was printed;
successful PASS/normal return is the supplied evidence.

Retained artifact and exact digest:

```text
Directory: /home/justin/temp/TaskCompassBackups/Phase1.2-schema1-linux-20261117-013703
Dump:      after-gui.sql
SHA-256:   83ae094ed33865bba0c22e39174f2977a49cc014de3f669115300105eec2bd26
```

This is the **original** dump from the failed-final-verification capture. Targeted
completion reported it unchanged; only the approved missing version/checksum
metadata was added, with no redump/re-export or modification of old capture files.
Earlier failure/confirmation/approval evidence remains above.

### Owner-confirmed private SQL review

Owner ran:

```bash
scripts/phase12/b72/04-review.sh \
   --backup-dir /home/justin/temp/TaskCompassBackups/Phase1.2-schema1-linux-20261117-013703
```

All evidence checksums again reported OK; the same dump digest printed. The pager
opened, owner confirmed `DUMP REVIEWED`, and the script reported:

```text
PASS: B7.2 checksums/comparisons and owner-confirmed private review.
STOP: no restore/migration is authorized.
```

**Linux B7.2 passed (owner-reported)**: private populated schema1 dump, complete
38-column legacy/history evidence, matching before/after and current-data checks,
final pinned schema verification, sealed checksums and owner-confirmed private SQL
review. SQL safety review is the owner's assertion; the agent has not read or
parsed the private dump. No dump/exports/secrets were added to the repository.
This backup has **not yet been restored**; B7.2 does not prove recovery or authorize
B7.3. Preserve every artifact, original Phase1.1 backup/export/restored database
and this directory unchanged.

### Remaining work and authority

Windows B7.2 remains approved for the normal operator-only four-stage procedure
but no Windows capture result is reported yet. The exceptional completion approval
was for the exact Linux retained directory, not a general Windows retry path.
Next finish/report Windows capture and review. Any failure stops for review.

No B7.3 import, grant changes, migrations, GUI writes, resets or cleanup are
approved. A later restore request must identify each actual platform's privately
reviewed dump hash, concrete target/account, empty-target preflight, exact import
command and preservation/recovery checks; the Linux hash is now available for that
future review. Do not rerun either capture/completion on this sealed directory.
Phase1.2 remains unaccepted and no Phase1.3 work is authorized.

### Reporting changes and validation

Updated milestone current status/append-only evidence, Roadmap and
TestingPhase1.2 pointers. Markdown local-reference/fence checks and git diff --check
passed. No application/source/script/SQL/migration/private-artifact changes or
agent live connection, completion, dump, restore or cleanup. Build/CTest not rerun
for this reporting-only update; prior offline results remain historical evidence.
No commit, push, merge or tag.


## 2026-11-17 — Windows B7.2 stopped before capture began

### Actual owner-reported checks and failure

Windows step01 passed reviewed-file checksums and reported installed native tools:
MariaDB13.0.2 client15.2 Win64 AMD64 (revision620a4b5624222048fece7fe1789978ae0675cae2)
and mariadb-dump13.0.2 client10.20 Win64 AMD64. Step02 root metadata report matches
B7.1: feldria-w10/13.0.2/3306, taskcompass_dev, lower_case_table_names1, FK/CHECK
checks1, utf8mb4/utf8mb4_unicode_ci, exactly five InnoDB tables. Version1 checksum
and stored fingerprint unchanged; appliedAt1794691232396; dataset
457bb52b-fea4-4347-9c47-675c6b035c90 with createdAt1794691232396. Counts remain
SchemaMigration1/Dataset1/Task2/CommandRecord16/LocalChange16, both tasks active
and not tombstoned. Exact approved account/grant report unchanged; no extra
roles/columns/stored-code/target-session rows displayed. PUBLIC rights still only
on unrelated test/test\_% scopes. Owner transcript, not agent SQL evidence.

Owner confirmed `BASELINE UNCHANGED; WRITERS CLOSED`. Step03 printed a proposed
fresh destination, then failed its **initial** taskcompass_migrate verification:

```text
Fresh destination: /c/Users/justin/TaskCompassBackups/Phase1.2-schema1-windows-20261117-020553
Cannot open MariaDB. Check service/TCP availability, QMYSQL, account, password and grants.
STOP: Read-only schema-1 verification failed. Preserve evidence; no retry, cleanup or repair.
```

Owner attributes this to another incorrect password. Root's preceding successful
TCP login does not validate the migrator password. No numeric capture exit was
supplied. Source review confirms verifySchema1 runs **before** invoking 09-backup,
which is responsible for directory creation. No helper-stage banner, dump/export
or artifact success was reported. Therefore this script invocation did not create
that directory, run a dump or write evidence. The agent has not inspected the
private path; do not remove anything if an unexpected path already exists.

### Reviewed next operator action within existing fresh-capture approval

This differs from Linux's final-verification failure: **no partial capture needs
completion**. Do not use 03a-finish-capture or 04-review for this failed attempt.
After this report/source review, an owner-initiated **fresh** normal B7.2 capture
is within the existing Windows approval; it selects a new timestamped directory
and replaces no artifact. This is not an automatic retry by the agent or an
approval to repair/reuse a failed capture. No extra target, privilege or operation
is added, and no guard is disabled.

If writers have remained closed and the immediately preceding baseline/grants are
still unchanged, owner may run:

```bash
scripts/phase12/b72/03-capture.sh --approved
```

Use the **Windows taskcompass_migrate** password at the first and final migrator
prompts (not Linux's password, root or diagnostic). The helper's taskcompass_app
and taskcompass_dev prompts also require their Windows account passwords. No
password reset/change is suggested. If writers resumed or state changed, repeat
step02 and review it before any new capture. If this new attempt encounters any
error, stop/report its exact location; do not loop or repair. Only after final
capture PASS may the normal step04 review proceed for its newly printed directory.

### Status, files and validation

Linux B7.2 remains passed with its sealed dump/digest unchanged. Windows B7.2 is
not passed: metadata/tools succeeded, actual capture/review pending. B7.3/restores,
migrations, account/grant changes, resets, GUI writes and cleanup remain unauthorized.
Updated milestone, Roadmap and TestingPhase1.2 reporting only. Markdown local links/
fences and git diff --check passed; no build/CTest rerun for documentation-only
changes. No agent SQL, capture, private-directory inspection, retry, repair or
artifact change. No source/script/SQL/migration edit, commit/push/merge/tag or
Phase1.3 work.


## 2026-11-17 — password entry retries and per-run credential reuse

### Latest Windows failure and owner direction

Owner reported another normal Windows capture attempt proposing:

```text
/c/Users/justin/TaskCompassBackups/Phase1.2-schema1-windows-20261117-023735
```

The initial taskcompass_migrate schema1 verification succeeded with dataset
457bb52b-fea4-4347-9c47-675c6b035c90. Existing helper 09-backup began and accepted
its writer confirmation, then failed opening MariaDB as taskcompass_app:

```text
Cannot open MariaDB. Check service/TCP availability, QMYSQL, account, password and grants.
STOP: Schema verification failed.
STOP: B7.2 operation failed (exit 1). Preserve evidence; no retry, cleanup or repair.
```

No explicit numerical exit was supplied. The helper's runtime verification is
before newDirectory, so this attempt created no capture directory or artifact.
Agent inspected source only, not the private Windows path. If a directory exists
unexpectedly, preserve it and report; do not delete it. Earlier Linux B7.2 success,
its sealed dump/hash and all previous failures/approvals remain unchanged.

Owner explicitly objected to single-chance/repeated password prompts. Applied this
as authorization to improve **credential entry only**, not authorization to repeat
failed backups, database changes or interrupted migrations. Bounded read-only
login re-entry is the narrow exception to previous blanket retry wording; all
operation fail-stop conditions remain. No live procedure is run on owner's behalf.

### Implemented behavior and boundaries

- Up to **three password entries per TaskCompass account**, with current platform,
  account and attempt number shown. Empty entry can be corrected; EOF cancels.
- Before caching, native client performs only `SELECT DATABASE(),CURRENT_USER()`
  on fixed taskcompass_dev at127.0.0.1:3306 with explicit fixed account. Only
  recognized native ERROR1045/SQLSTATE28000 denial allows another probe. It means
  authentication rejection, not proof that password alone is the cause.
- A successful probe must return exactly the approved database/authenticated account.
  Identity mismatch, network/other/unknown errors or malformed output stop without
  retry. Known observed MariaDB password-option warning is explained separately,
  never treated as identity data or suppressed by changing TLS options. Native
  error text is not echoed, to avoid exposing candidate credentials/connection data.
- Checked account-specific passwords remain in a nonexported shell associative
  array for that process only. A normal capture asks once each for migrator,
  runtime and diagnostic; all three are checked before directory creation. Final
  verify and all extra exports reuse them, eliminating the late password prompt.
  Cache is cleared on exit/cancellation and never written to configuration/artifacts.
- Existing backup helper is unchanged. After explicit writer confirmation, a shell
  builtin sends that confirmation and cached app/diagnostic passwords over an
  anonymous pipe to its existing stdin interface. No secrets in command arguments,
  saved files, history or stdout. Its schema/identity checks, private ACL/umask,
  dump settings, comparisons and failure behavior are preserved. Subsequent native
  children receive only their account's password through existing MYSQL_PWD
  mechanism; Windows environment conversion exclusions remain in place.
- Qt error redaction now covers all cached accounts. Native errors after login
  checks, Qt/plugin/schema errors, dump/export failures and checksum mismatches
  still stop immediately: **no operation retry**, re-export, overwrite or repair.
  Native login success followed by Qt failure is not presumed another password typo.
- Linux sudo and Windows native root password prompts remain unchanged, avoiding
  administrator-password capture/caching. The new three-attempt feature applies
  to taskcompass_migrate/app/dev in B7.2, not a global credential manager or other
  milestones' scripts. No accounts/passwords/grants changed or new dependencies.
- Explicit completion uses the same account entry behavior, but retains its own
  exact-target separate approval and no-partial-reseal guards; no new completion
  is authorized by this UI improvement. Existing Linux capture remains sealed.

### Files and actual offline validation

Modified scripts/phase12/b72/run.sh, tests/Phase12BackupScripts.test.sh,
docs/Phase1.2B72.md and current milestone/Roadmap/TestingPhase1.2 status. CMake,
production C++, migrations/SQL review package and old Phase1.1 helper untouched.
No server access or actual backup directory inspected/modified by the agent.

```bash
bash -n scripts/phase12/b72/run.sh
bash tests/Phase12BackupScripts.test.sh
ctest --test-dir build --output-on-failure \
   -E '^(MariaDbIntegrationTests|OrganizationIntegrationTests)$'
sha256sum --check scripts/phase12/b7/ReviewChecksums.sha256
git diff --check
```

Expanded fake-executable harness passed **seven PASS groups**. New cases exercise
wrong then correct secrets on second/third entry, exactly three-attempt exhaustion,
EOF cancellation, per-account distinct passwords/cache reuse through helper and
exports/final verification (no extra prompt input supplied), native warning
handling, no network/identity retry and no artifact creation on prelogin failure.
Mock Windows flow additionally exercises a corrected app password, CRLF identity/
error output and special-character secrets (quotes, backslashes, ampersands/path-like
text) with conversion exclusions, cache separation and redaction. Existing dump
failure/drift/partial-completion/manifest/ACL guards still pass. Root Windows
workflow remains native --password with no script-captured admin secret.

Full Linux offline suite passed **17/17**, exit0, 7.33 seconds; both live suites
explicitly excluded, not executed. Final direct harness rerun after adding extra
mock-Windows cases also passed all seven groups. Shell syntax, reviewed checksum
manifest, Markdown references/fences/whitespace and git diff --check passed.
No application/C++ build needed for shell/test/docs-only changes. Actual Windows
native authentication retry/cache execution is still pending owner results;
mocked Windows checks are not relabeled as native validation.

No agent live authentication/SQL, dump/export, backup/restore, provisioning/grants,
reset/repair/cleanup, dependency installation or commit/push/merge/tag. Only
harness-owned temporary fake artifacts were removed as normal offline test cleanup.

### Next operator step within existing Windows B7.2 approval

Copy/update the scripts in the Windows checkout through owner's normal source
workflow; no C++ rebuild is required. After unchanged baseline review and stopped
writers, run one fresh normal capture:

```bash
scripts/phase12/b72/03-capture.sh --approved
```

It selects a new independent timestamped destination; previous failed attempts
stopped before directory creation, so no completion script is applicable. Each
TaskCompass password may now be corrected within three login-only entries, and
once accepted is not requested again during that run. If three entries fail or
any operation fails, stop/report; no whole-operation loop or repair. Step04 only
after final capture PASS. Linux evidence remains preserved, Windows B7.2 capture/
review incomplete, B7.3 and all database mutations/restores still separately gated.


## 2026-11-17 — Windows B7.2 capture/review passed; both baselines sealed

### Owner-reported native Windows capture

Owner ran the updated 03-capture.sh --approved under UCRT64 with the unchanged-
baseline/writers-closed confirmations. All reviewed package checksums passed.
Each Windows TaskCompass credential authenticated on attempt1/3; memory-only
reuse succeeded through the existing helper, additional exports and final verify,
with no late password re-entry. The authentication-denial retry branch is still
mock-tested only; this run demonstrates native checked-credential/cache behavior,
not an unreported Windows denied-login/re-entry experiment. Client password-option
notices appeared as explained; no TLS settings changed or negotiated TLS claimed.

Both pinned migrator verifications and the helper's app verification reported
schema1/dataset457bb52b-fea4-4347-9c47-675c6b035c90. Helper reported backup exit0,
nonempty dump/hash recorded and consistent source exports. All checksums passed
for after-gui.sql, source.tsv, legacy-before.hex.tsv, legacy-after.hex.tsv,
schema1.ddl.txt, source-after.tsv, dataset.txt, server.txt, client-version.txt and
dump-version.txt. Final result:

```text
PASS: B7.2 capture, comparisons and checksums. Private SQL review still pending.
Directory: /c/Users/justin/TaskCompassBackups/Phase1.2-schema1-windows-20261117-064917
```

### Wrong-parent guard and successful private review

First 04-review invocation selected only `/c/Users/justin/TaskCompassBackups`, the
parent directory. Read-only ACL validation rejected it with `Private capture ACL
validation failed`. No private capture file, permission, hash or database was
changed by that review action; inspection stopped before artifact checks/pager.
This is an operator path-selection error, not evidence that capture's ACL was bad.
No ACL repair or broadened access was performed or proposed.

Owner then supplied the full newly created capture directory. ACL validation,
all artifact checksums and comparisons succeeded; the same dump digest printed.
Owner inspected the pager and entered `DUMP REVIEWED`, followed by:

```text
PASS: B7.2 checksums/comparisons and owner-confirmed private review.
STOP: no restore/migration is authorized.
```

**Windows B7.2 passed (owner-reported)**. No separate numeric wrapper exit was
printed; the supplied successful PASS messages and normal returns are the result
evidence. Private SQL review is owner-confirmed; agent did not inspect the dump.
Both Linux and Windows B7.2 are now complete, with all previous failures retained.
This is not final Phase1.2 acceptance or proof of either new snapshot's restore.

### Retained exact snapshots (do not alter)

| Platform | Directory | after-gui.sql SHA-256 | Dataset |
| --- | --- | --- | --- |
| Linux | /home/justin/temp/TaskCompassBackups/Phase1.2-schema1-linux-20261117-013703 | 83ae094ed33865bba0c22e39174f2977a49cc014de3f669115300105eec2bd26 | 21be9da7-b767-4578-b972-cc9a35122ddb |
| Windows | /c/Users/justin/TaskCompassBackups/Phase1.2-schema1-windows-20261117-064917 | 6588eb9ebc30b63b36b33f2abbc4a0de9da31b252c41a3fd585b8b060073fe50 | 457bb52b-fea4-4347-9c47-675c6b035c90 |

Linux baseline counts Task4/CommandRecord28/LocalChange28; Windows Task2/16/16;
each Dataset1/SchemaMigration1. All 38 original columns, history and tombstones
are included in canonical evidence. Paths are private operator artifact references,
not repository content. Preserve every capture/export/checksum/version file, all
Phase1.1 reports/artifacts and original restored taskcompass_integration databases.
Do not rerun capture/completion over these sealed directories or reuse them as
scratch space. Future comparison exports require new filenames, never overwrites.

### Approval Requested — B7.3 populated schema-1 isolated restore/rehearsal

Now request **operator-only B7.3** for both exact local snapshots above, using the
existing [B7.3 reviewed procedure](../Phase1.2B7.md#b73--populated-schema-1-recoveryrehearsal-clone).
This is a new live database approval; B7.2 does not authorize it. Do not execute
until owner approves the concrete targets/artifacts and procedure below.

- Linux target: feledria/MariaDB10.5.29, local127.0.0.1:3306,
  **taskcompass_phase12_test**, existing taskcompass_phase12_test@127.0.0.1.
- Windows target: feldria-w10/MariaDB13.0.2, local127.0.0.1:3306,
  **taskcompass_phase12_test**, existing same-named account@127.0.0.1.
- Each uses only its own platform's reviewed after-gui.sql, bound to the exact
  directory/digest above. No cross-platform/version import or dump edits.
- Fresh root metadata-only preflight using preflight-rehearsal.sql and
  preflight-clone-grants.sql; Windows additionally public-grants.sql. Require
  correct server/encoding, **entirely empty target** (no tables/views/stored code/
  unexpected sessions), exactly existing six schema-scoped test privileges and
  no extra hosts/global/table/column/role/PUBLIC authority covering TaskCompass.
  Verify all private artifact checksums and owner SQL review again before import.
  Unknown/nonempty target or changed artifact/account/server stops; **no reset**.
- Root only inspects metadata. Import uses the dedicated test account; no account,
  password, authentication or grant changes. The import executes only the exact
  privately reviewed dump's five CREATE TABLE definitions, INSERT rows and normal
  session prologue/epilogue. No migration002 or new Dataset initialization. No
  --force, --databases, source-qualified writes, DROP, repair or repeated import.

Exact proposed Linux import (after preflight/checksum gates):

```bash
/usr/bin/mariadb --no-defaults --protocol=TCP --host=127.0.0.1 --port=3306 \
   --user=taskcompass_phase12_test --password --default-character-set=utf8mb4 \
   --database=taskcompass_phase12_test \
   < /home/justin/temp/TaskCompassBackups/Phase1.2-schema1-linux-20261117-013703/after-gui.sql
```

Exact proposed Windows import (after its own preflight/checksum gates):

```bash
'/c/Program Files/MariaDB 13.0/bin/mariadb.exe' \
   --no-defaults --protocol=TCP --host=127.0.0.1 --port=3306 \
   --user=taskcompass_phase12_test --password --default-character-set=utf8mb4 \
   --database=taskcompass_phase12_test \
   < /c/Users/justin/TaskCompassBackups/Phase1.2-schema1-windows-20261117-064917/after-gui.sql
```

Approval would also include the runbook's post-import read-only verification and
new private rehearsal1.hex.tsv/rehearsal1.ddl.txt exports in each snapshot directory
(no overwrite), full 38-legacy-column/count/hash equality against its sealed capture,
same-platform raw DDL equality, and guarded Qt `--verify --phase12-test --to-version 1`
with explicit TASKCOMPASS_PHASE12_TEST_DB_* environment. Expect imported dataset
UUID exactly the source UUID, original counts/history/payload bytes and schema1/
fingerprint verified. Clones remain passive rehearsal copies, never sync clients.

Risk/recovery: MariaDB DDL import is not a rollback bundle and may leave partial
state. Stop immediately on any nonzero client status, metadata/hash/content mismatch
or uncertain result; preserve dump, target and diagnostics for explicit review.
Do not retry, reimport, reset, remove objects, rewrite history or broaden grants.
No universal DROP/recovery SQL is authorized; actual-state-specific recovery would
require a fresh review. Preserve populated clones afterward; never run the reset-
capable 04-integration-test.sh suite against them.

Excluded: all taskcompass_dev writes, original taskcompass_integration access/reset,
provisioning/grant changes, migration002, narrow-role swaps, GUI writes, cleanup,
B7.4+ and Phase1.3. B7.4 narrow-migrator/runtime rehearsal still needs separate
approval after this populated recovery gate passes. No restore run by the agent.

### Reporting validation

Updated milestone/Roadmap/TestingPhase1.2 status only. Markdown local-reference/
fence checks, digest-format checks and git diff --check passed. No application/
script/SQL/migration/private-artifact change or agent database/backup/restore access.
Build/CTest not rerun for reporting-only edits; actual earlier offline results
remain preserved. No dependency installation, commit/push/merge/tag or cleanup.


## 2026-11-17 — B7.3 approved; numbered restore/rehearsal scripts prepared

### Explicit owner approval and execution boundary

Owner answered **"yes"** to the preceding exact-artifact/target B7.3 approval
request for both platforms. This authorizes only the documented owner-run schema1
snapshot import into existing empty taskcompass_phase12_test, current grants /
identity / checksum gates and full preservation verification. Both exact source
paths/digests/dataset IDs remain the preceding table's immutable bindings.
**No live SQL/private-artifact access or restore has been executed by the agent.**
Prior owner instruction to provide operator commands rather than execute on their
behalf persists. No B7.4 grant swap/migration, dev write, original integration access,
provisioning, reset, cleanup, GUI write, Phase1.3 or commit/push/merge/tag permission.

### Exact operator commands (one at a time, same on both platforms)

Copy all changed scripts/shared helper into each checkout through owner's normal
source workflow. From repository root, Linux Bash or Windows UCRT64, keep all
writers stopped and leave MariaDB running:

```bash
scripts/phase12/b73/01-preflight.sh --approved
scripts/phase12/b73/02-import.sh --approved
scripts/phase12/b73/03-compare.sh --approved
```

Run step2 only after step1 PASS; step3 only after import PASS. A failure stops for
review, not an automatic retry/reset or return to an earlier step. Detailed prompt,
password, expected result and failure instructions: [Phase1.2B73.md](../Phase1.2B73.md).
No setup variables or long pasted blocks needed. Defaults select the exact approved
local snapshots/hosts/versions and fixed target/account. Actual owner results pending.

Step1 validates private permissions and complete sealed capture through the existing
B7.2 evidence checker in explicit checks-only mode, then the exact platform dump
hash and native client version/path against sealed metadata. Root metadata checks
are read-only: sudo/socket on Linux or native --password/TCP on Windows, no cached
administrator credentials. Require expected selected target/root/server/version,
case mode0/1 and FK/CHECK enforcement1, correct existing charset/collation, zero
schema objects/stored code/other selected-target sessions, exact test account host,
exact six schema grants and no global/table/column/role extras or PUBLIC write
coverage of source/target. Nonempty/unknown/malformed output fails closed, never reset.

Step2 binds the same pinned dump hash, requires `RESTORE taskcompass_phase12_test`
and `DUMP REVIEWED; WRITERS CLOSED`, authenticates the exact test account (three
login-only attempts with memory cache), repeats fresh root preflight, and rechecks
snapshot. Before any import SQL it creates private **b73-import-attempt.txt** with
the pinned digest using noclobber. Import runs once under test credentials against
only taskcompass_phase12_test. No root import, --force, DDL reset or migration apply.
On native exit0 only, creates **b73-import-success.txt** and prints PASS. Any prior
attempt/success/comparison evidence blocks reimport. Do not delete/rename/copy
around markers. Even an apparently empty target after uncertain import needs review.
No new accounts or privileges; six test rights remain as previously provisioned.

Step3 requires matching attempt/success markers, authenticates test account, runs
Qt `--verify --phase12-test --to-version 1` with explicit approved test environment
and expected source UUID, then creates only new rehearsal1.hex.tsv/rehearsal1.ddl.txt
and b73-comparison.sha256. Full 38-original-column/count/immutable payload/tombstone/
time/history/migration-row equality and raw same-platform DDL equality are required
against sealed baseline, with verified comparison checksums. Qt preserves strict
shape/FK endpoint/index/check/fingerprint and dataset guards. No existing capture
file is modified. Failed/partial comparison remains for review; no automatic rerun.

Expected outputs: preflight PASS for correct empty target/exact rights; import
`PASS: one-shot import exit 0`; comparison schema1/source UUID and
`PASS: B7.3 schema1/UUID/all legacy columns and history/counts/DDL match exact snapshot.`
The clone intentionally shares its source dataset UUID as a passive rehearsal copy,
not a syncing client. Preserve it and all evidence; never run reset-capable tests
on the now-populated target. Restore failure may leave nontransactional partial
DDL; stop, preserve state, request actual-state-specific recovery approval. No
DROP/suffix-resume/repair permission follows from B7.3 approval.

### Files, reuse and architecture decisions

- New scripts/phase12/b73/01-preflight.sh, 02-import.sh, 03-compare.sh and run.sh.
- New scripts/phase12/b73/preflight.sql: eight **SELECT-only** metadata assertions
  over the authorized server/target/account; Windows additionally uses existing
  public-grants.sql. Same approved preflight authority as the reviewed report files,
  expressed as tagged scalar rows for automatic guards, no added SQL mutation scope.
- New scripts/phase12/read-only-login.sh factors the existing bounded login/cache/
  redaction functions shared with B7.2. Caller database/account pairs are hardcoded
  to dev's existing three accounts or the exact phase12 test pair; no general
  target/credential abstraction or admin-password capture. B7.2 behavior/guards
  remain otherwise unchanged and regression tested.
- Modified B7.2 run.sh to source that helper and add review-only --checks-only:
  integrity/permissions/UUID checks without pager or a new SQL-safety assertion.
  This cannot bypass import's reviewed-dump confirmation or pinned owner-reviewed
  bytes. --checks-only on any live/capture action is rejected.
- New tests/Phase12RestoreScripts.test.sh; CMake registers Phase12RestoreScripts
  only when existing Bash/Python3 test prerequisites are available, with the existing
  platform timeout and explicit detected Python interpreter for portable mocks.
- Modified Phase12BackupScripts.test.sh covers checks-only; new docs/Phase1.2B73.md
  and updated B7.2 guide, milestone/Roadmap/TestingPhase1.2 status/links.
- No production C++/GUI/domain/application changes, migration SQL, diagrams,
  DatabaseConfig allowlists, dependency installs or existing validation helper
  grant/DDL behavior changes. Private comparison/attempt artifacts contain no secrets.

New markers are private evidence/one-shot safety, not a schema/history change or
a substitute for actual state checks. Script copies of identical snapshot bytes
still cannot override pinned digest/server/target; a location change requires its
own authorization. The agent's offline fixtures use simulated hashes via a fake
hash utility; no production test bypass or digest override was introduced.

### Actual agent-executed offline validation

```bash
bash -n scripts/phase12/read-only-login.sh
bash -n scripts/phase12/b73/run.sh
bash tests/Phase12BackupScripts.test.sh
bash tests/Phase12RestoreScripts.test.sh
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure \
   -E '^(MariaDbIntegrationTests|OrganizationIntegrationTests)$'
ctest --test-dir build --output-on-failure \
   -R '^(Phase12BackupScripts|Phase12RestoreScripts)$'
sha256sum --check scripts/phase12/b7/ReviewChecksums.sha256
git diff --check
```

CMake/build succeeded. Full Linux offline suite **18/18 passed**, exit0,22.20sec;
both live integration suites explicitly excluded, not run. After tightening native
client-version/table-case/FK/CHECK preflight metadata and portable Python mock
selection, final focused CTest **2/2 passed**,12.42sec (backup2.29/restore10.12).
No test failures occurred during this B7.3 source preparation. All script syntax,
Markdown links/fences/whitespace, 20 reviewed checksums and git diff --check passed.
Protected migration001/002 and accepted Phase1.1.md SHA-256 values unchanged as
recorded above; no diagram regeneration required for read-only tooling SQL.

New four-group fake-executable harness covers no-approval/help/preview zero
connections; native client mismatch, nonempty/wrong-server/extra-role rejection
before marker/import; exact pinned snapshot/import/Qt/full comparison positive
flow; attempt marker retention on partial failure; no reimport/reset/overwrite;
tampered file and valid local checksum but wrong approved digest refusal; mocked
Windows ACL/pinned snapshot/PUBLIC scope and restored UUID rejection. It asserts
root preflight SQL is SELECT-only and fake clients reject other DB/account targets
or --force; fake verifier accepts only verify1/phase12-test, never apply/run/reset.
Actual hash utility behavior is simulated only for exact synthetic fixture bytes;
real repository checksums are calculated normally by that test double. No private
snapshot data or real server was used. Existing B7.2 seven-group harness still
passes with the shared helper and new no-pager checks-only guard.

Actual Windows shell/native-client/ACL/import and Linux live restore execution are
**pending owner**, not established by mocks. Python is only for an existing-style
offline test fixture, not the runtime operator scripts or a new installed dependency.
No real database connection, backup/import/export, private-file inspection,
provisioning/privilege or destructive operation was executed by the agent. Only
harness-owned temporary fixtures were cleaned. No commit, push, merge or tag.

### Remaining work and next checkpoint

Owner runs/reports each stage separately per platform, preserves source snapshots,
new evidence and populated clones. No automatic execution/resume or additional
B7.3 approval needed for the already approved exact scope. Unexpected state stops
for review. B7.4's narrow migration/runtime role changes and migration002 need
separate concrete approval after populated restoration/preservation passes on both
servers. Development upgrade/GUI acceptance/schema2 recovery and final acceptance
remain later, separately gated work. No Phase1.3 begins.


## 2026-11-17 — Linux B7.3 populated restore/preservation passed

### Actual owner-reported stage results

Owner ran all three approved numbered B7.3 scripts on Linux:

```bash
scripts/phase12/b73/01-preflight.sh --approved
scripts/phase12/b73/02-import.sh --approved
scripts/phase12/b73/03-compare.sh --approved
```

Step01 verified all reviewed package and private B7.2 evidence checksums, the exact
approved Linux dump digest and sealed native-client metadata. Read-only root
preflight reported feledria/10.5.29-MariaDB, empty fixed test target, exact six
scoped grants and no roles/extras/PUBLIC authority. These are operator-script
assertions reported by the owner, not agent-executed SQL/catalog inspection.
The reused B7.2 `Stage 04/04` banner is the evidence-check-only helper, not a new
interactive private review or any backup recreation.

Step02 accepted both `RESTORE taskcompass_phase12_test` and
`DUMP REVIEWED; WRITERS CLOSED`. The existing Linux test account authenticated on
attempt1/3. Fresh automatic root preflight again passed empty-target/grant guards
immediately before import, and all snapshot checks were repeated. Reported:

```text
PASS: one-shot import exit 0. Next run 03-compare.sh; do not run integration/reset tests.
```

No database reset, account or grant change, migration002 or dev write was part of
these scripts. The approved dump alone was imported under the existing test account.

Step03 repeated private source-evidence checks, authenticated the same test account
on attempt1/3 and reported:

```text
Schema 1 verified; dataset 21be9da7-b767-4578-b972-cc9a35122ddb
rehearsal1.hex.tsv: OK
rehearsal1.ddl.txt: OK
b73-import-attempt.txt: OK
b73-import-success.txt: OK
PASS: B7.3 schema1/UUID/all legacy columns and history/counts/DDL match exact snapshot.
Preserve populated clone; no reset, migration or grant changes authorized.
```

**Linux B7.3 passed (owner-reported).** This proves recovery of this exact populated
schema1 snapshot on the approved Linux server, with guarded schema/fingerprint/FK/
UUID verification, all 38 original columns/counts/history/payload bytes/tombstones
and same-platform DDL equality against the sealed source evidence. Clone UUID
intentionally remains the source UUID. Baseline source counts preserved by the
full comparison: Dataset1, SchemaMigration1, Task4, CommandRecord28, LocalChange28;
no separate per-table numeric query was pasted in this run. Native import exit0
was explicitly reported; other stage success is evidenced by their PASS output.
No new backup/schema2/narrow-role/migration acceptance is implied.

### Retained targets and artifacts

- Passive populated clone: taskcompass_phase12_test on feledria/10.5.29, local
  TCP127.0.0.1:3306; existing taskcompass_phase12_test@127.0.0.1.
- Snapshot/evidence directory remains
  /home/justin/temp/TaskCompassBackups/Phase1.2-schema1-linux-20261117-013703.
- Unchanged approved after-gui.sql digest:
  83ae094ed33865bba0c22e39174f2977a49cc014de3f669115300105eec2bd26.
- New private import attempt/success markers, rehearsal1.hex.tsv/rehearsal1.ddl.txt
  and b73-comparison.sha256 retained. Export/checksum contents are private and
  were not supplied to or read by the agent.

**Do not rerun preflight/import/comparison on the completed Linux stage or run
reset-capable integration tests against this populated clone.** Preserve every
source snapshot, new evidence, all Phase1.1 artifacts and original restored
integration databases. No cleanup is authorized. B7.4 requires separate exact SQL/
privilege/migration approval before changing this clone.

### Remaining approved platform check and reporting validation

Windows B7.3 is still approved for owner-only execution using its own pinned
snapshot and database/server. Run the same three scripts there one at a time;
stop on failure, use Windows test account password and report outputs. Never use
the Linux snapshot/UUID/digest for Windows. No new B7.3 approval required for the
existing exact Windows scope; B7.4+ remains separately gated after both pass.

Updated current milestone/Roadmap/TestingPhase1.2 status and appended this evidence.
Markdown local-reference/fence checks and git diff --check passed. No code, script,
SQL, migration, private file or database operation performed by the agent; no
application build/CTest rerun for reporting-only changes. All live observations
above are owner-reported, distinct from prior agent offline tests. No commit,
push, merge, tag, dependency install, Phase1.3 or acceptance claim.


## 2026-11-17 — Windows B7.3 passed; populated recovery gate complete

### Actual owner-reported native Windows results

Owner ran all three B7.3 stages in UCRT64 on feldria-w10:

```bash
scripts/phase12/b73/01-preflight.sh --approved
scripts/phase12/b73/02-import.sh --approved
scripts/phase12/b73/03-compare.sh --approved
```

Step01 passed reviewed-file and full private-source evidence checks, exact pinned
snapshot/native-client binding, and read-only root preflight:

```text
PASS: feldria-w10 / 13.0.2-MariaDB; empty fixed target, exact six grants, no roles/extras/PUBLIC writes.
```

The reused B7.2 review banner is evidence-check-only mode, explicitly not another
pager/SQL review. Source after-gui.sql digest remained
6588eb9ebc30b63b36b33f2abbc4a0de9da31b252c41a3fd585b8b060073fe50, in
/c/Users/justin/TaskCompassBackups/Phase1.2-schema1-windows-20261117-064917.

Step02 accepted both restore/writers-reviewed confirmations. Windows
taskcompass_phase12_test authenticated on attempt1/3; fresh root metadata preflight
and snapshot checks passed again before import. Reported native import exit0:

```text
PASS: one-shot import exit 0. Next run 03-compare.sh; do not run integration/reset tests.
```

Step03 authenticated the same test account on attempt1/3 and reported:

```text
Schema 1 verified; dataset 457bb52b-fea4-4347-9c47-675c6b035c90
rehearsal1.hex.tsv: OK
rehearsal1.ddl.txt: OK
b73-import-attempt.txt: OK
b73-import-success.txt: OK
PASS: B7.3 schema1/UUID/all legacy columns and history/counts/DDL match exact snapshot.
Preserve populated clone; no reset, migration or grant changes authorized.
```

**Windows B7.3 passed (owner-reported).** Guarded Qt schema1/fingerprint/FK/index/
UUID checks and all 38 original columns/counts/audit payload bytes/history/timestamps/
tombstones plus raw same-platform DDL matched the sealed Windows source. Baseline
counts preserved through full comparison are Dataset1/SchemaMigration1/Task2/
CommandRecord16/LocalChange16; no fresh numeric row-count query was separately
pasted. Import exit0 was explicit; remaining stages' PASS/normal return is the
reported result, without a separate numeric wrapper exit code.

Native password-option notices appeared but did not prevent success. No TLS option
was changed and actual negotiated TLS remains unmeasured. No failed import/retry,
reset, grant change or metadata repair was reported. All observations above are
owner-executed results; agent did not access a server or private export/dump.

### Both populated schema-1 rehearsals retained

B7.3 is now passed on **both supported servers**, proving recovery of each exact
reviewed populated schema1 baseline. Retain both taskcompass_phase12_test clones,
original source snapshot/evidence directories, new import markers and rehearsal1
comparison exports/manifests unchanged. Both clone UUIDs intentionally match their
respective source, not each other. Linux dump digest remains
83ae094ed33865bba0c22e39174f2977a49cc014de3f669115300105eec2bd26. All prior failures,
approvals and Phase1.1 restored databases/artifacts remain preserved. No reset-
capable integration tests or repeated B7.3 import/comparison on populated clones.
This is not narrow-role migration validation, development upgrade or final acceptance.

### Approval Requested — B7.4a narrow-migrator populated clone upgrade only

Request new **operator-only B7.4a** authorization on both exact existing clones:

| Platform | Host / server | Database and account | Current expected UUID / version |
| --- | --- | --- | --- |
| Linux | feledria / 10.5.29-MariaDB / local127.0.0.1:3306 | taskcompass_phase12_test / taskcompass_phase12_test@127.0.0.1 | 21be9da7-b767-4578-b972-cc9a35122ddb / schema1 |
| Windows | feldria-w10 / 13.0.2-MariaDB / local127.0.0.1:3306 | taskcompass_phase12_test / taskcompass_phase12_test@127.0.0.1 | 457bb52b-fea4-4347-9c47-675c6b035c90 / schema1 |

Full reviewed operator procedure: [B7.4a](../Phase1.2B7.md#b74a-migration-role).
Source-only preparation/numbered scripts can follow the existing operator-script
pattern; no agent live execution. Before any grants: stop all writers, revalidate
all exact snapshot/import/comparison evidence, current schema1/checksum/fingerprint/
dataset and all legacy values against the sealed source, and root metadata for
unchanged host/version/charset/objects and exactly the current six schema privileges,
with no roles/column/table/global/PUBLIC extras. Baseline drift stops for review.
Do not overwrite existing B7.3 exports or assume an old PASS is fresh state proof.

Exact proposed privilege transition, unchanged
[rehearsal-migration-grants.sql](../../scripts/phase12/b7/rehearsal-migration-grants.sql):

```sql
-- SPDX-License-Identifier: GPL-3.0-or-later
-- B7 review package only: execution requires the matching explicit approval.
-- Only after approved snapshot import and verified exact six existing schema grants.
REVOKE SELECT, INSERT, UPDATE, CREATE, DROP, ALTER ON `taskcompass\_phase12\_test`.* FROM 'taskcompass_phase12_test'@'127.0.0.1';
GRANT SELECT ON `taskcompass\_phase12\_test`.* TO 'taskcompass_phase12_test'@'127.0.0.1';
GRANT CREATE ON taskcompass_phase12_test.Pillar TO 'taskcompass_phase12_test'@'127.0.0.1';
GRANT CREATE ON taskcompass_phase12_test.Project TO 'taskcompass_phase12_test'@'127.0.0.1';
GRANT CREATE ON taskcompass_phase12_test.Objective TO 'taskcompass_phase12_test'@'127.0.0.1';
GRANT CREATE, INSERT, UPDATE ON taskcompass_phase12_test.MigrationAttempt TO 'taskcompass_phase12_test'@'127.0.0.1';
GRANT ALTER ON taskcompass_phase12_test.Task TO 'taskcompass_phase12_test'@'127.0.0.1';
GRANT ALTER ON taskcompass_phase12_test.CommandRecord TO 'taskcompass_phase12_test'@'127.0.0.1';
GRANT ALTER ON taskcompass_phase12_test.LocalChange TO 'taskcompass_phase12_test'@'127.0.0.1';
GRANT INSERT ON taskcompass_phase12_test.SchemaMigration TO 'taskcompass_phase12_test'@'127.0.0.1';
```

This removes the broad six test-schema rights and retains schema SELECT plus the
8 narrowly scoped table grants. No password/account/authentication change, global
rights, GRANT OPTION, table DROP or standalone INDEX privilege. ALTER itself can
perform more than this migration, so it is strictly maintenance-only. Validate
this exact role set before invoking migration; if any GRANT/REVOKE fails or creates
an unexpected partial set, stop/inspect and request exact recovery, not broaden it.
Granting CREATE for absent new tables and these minima remain **unproven on the
populated servers**; this gate exists to establish them on both, not assume success.

Exact actual migration SQL is
[002_OrganizationalManagement.sql](../../migrations/mariadb/002_OrganizationalManagement.sql),
SHA256 **7de311935843e4fc04cbca74d025d9f38986a4154d70bdd29b35a2820f87b736**,
all nine reviewed DDL statements (four CREATE TABLE, five ALTER TABLE). Migration001
is not edited or reapplied. The runner journals the step/state/checksum/baseline,
validates shape and unchanged legacy digest, and records migration row2 only under
its controlled protocol. Never execute the SQL suffix/file manually or rewrite
SchemaMigration/MigrationAttempt to force verification.

Under the test account with explicit approved TASKCOMPASS_PHASE12_TEST_DB_* config,
the existing CLI operation sequence proposed is:

```bash
"$taskCompassMigrate" --verify --phase12-test --to-version 1 &&
"$taskCompassMigrate" --apply --phase12-test --to-version 2 &&
"$taskCompassMigrate" --verify --phase12-test --to-version 2
```

No --integration, dev credentials or root migration. Expected intentional changes:
five-table schema1 becomes nine-table schema2, four new tables (MigrationAttempt,
Pillar, Project, Objective), optional Task owner columns and typed history targets/
constraints/index changes from actual 002. Existing UUIDs, all 38 original columns,
counts, task timestamps/lifecycle/tombstones and raw historical payloadVersion1
JSON/audit/compensation data remain unchanged. Row1 metadata tuple unchanged; one
completed version2 attempt at step9 and schema row2/checksum/fingerprint verified;
new organizational tables empty and old new-owner/target columns NULL as specified.

After successful migration, approval includes new private rehearsal2-legacy.hex.tsv
and rehearsal2.ddl.txt exports (no overwrite), comparison to sealed baseline, and
read-only upgrade-postconditions.sql / schema2-health.sql checks: zero violations
for every actual CHECK/20 FKs and zero unexpected legacy owner/target values.
All guards/checks must pass before any further role swap or runtime work. This
approves no domain record edits or SQL runtime permission probe yet.

Backup/recovery: exact schema1 dumps and comparison evidence on both platforms are
retained and have now been successfully restored; leave them and every Phase1.1
artifact untouched. DDL/grants are not one reversible transaction. Any unexpected
or interrupted/uncertain grant/DDL/journal/final commit/verification is a hard stop:
preserve partial clone/attempt/schema history and diagnostics, no resume, suffix
execution, reset, overwrite, down migration or metadata repair. Read-only failure
inspection uses the fixed-target metadata procedure; actual-state-specific recovery
requires new explicit approval. A fresh schema1 recovery target or in-place clone
restore is **not** authorized here; no generic DROP/reimport plan is safe before
knowing the failure state. The original backup is preserved regardless of outcome.

B7.4a intentionally upgrades the two populated **rehearsal** clones while preserving
all original domain/audit data. It does not upgrade taskcompass_dev, change its
accounts/grants, affect original restored taskcompass_integration, provision a new
user/database, run destructive tests or cleanup, launch GUI, or start Phase1.3.
B7.4b runtime-equivalent role/probe is a separate later approval after this gate.
B7.5 development grants/migration remains separately gated after both populated
narrow-role rehearsals succeed. No final milestone acceptance is implied.

### Reporting changes and validation

Updated milestone/Roadmap/TestingPhase1.2 status and appended owner Windows evidence/
next concrete SQL/procedure approval request. Markdown local links/fences and git
diff --check passed. No script/code/SQL/migration/diagram/private-artifact change,
agent live query/grant/migration/backup/restore/reset or dependency installation.
Build/CTest not rerun for this reporting-only update; prior actual offline evidence
remains preserved. No commit, push, merge or tag. Await owner approval for B7.4a.

## 2026-11-17 — B7.4a approval recorded and numbered operator package prepared

### Status, scope and approval decision

Owner explicitly approved B7.4a on both Linux and Windows: populated fixed rehearsal
clones only, exact narrow privilege transition, migration002 and preservation
verification, conditional on fresh snapshot/schema1/UUID/data/server/grant checks.
Owner requested operator commands, not agent execution. This supersedes the
preceding approval request; it does not authorize B7.4b, B7.5, development writes,
GUI acceptance, provisioning, reset/recovery or Phase1.3. Live B7.4a results remain
**not attempted by agent / pending owner execution**. All historical reports above
and Phase1.1 evidence remain unchanged.

### Implemented source and review files

- Added `scripts/phase12/b74a/01-preflight.sh`, `02-grants.sh`, `03-migrate.sh`,
  `04-preservation.sh`, shared `run.sh` and SELECT-only `preflight.sql`.
- Added `tests/Phase12MigrationRehearsalScripts.test.sh`; reused the offline B7.3
  fixture factory through a test-only hook in `tests/Phase12RestoreScripts.test.sh`.
- Registered the offline test in `CMakeLists.txt` with finite Linux/Windows timeouts.
- Added `--skip-reconnect` to the shared read-only authentication probe in
  `scripts/phase12/read-only-login.sh`. Authentication retries remain bounded to
  three read-only probes; mutations are never retried.
- Added [B7.4a operator guide](../Phase1.2B74a.md); updated current status in this
  milestone, Roadmap and TestingPhase1.2. No application C++ changes, migrations,
  diagrams, reviewed SQL, backup artifacts or private configuration changed.

The scripts pin each platform's previously reviewed snapshot path/digest, server,
UUID and original schema1 data/DDL. They verify source SQL and the built runner's
embedded SQL before connecting; B7.2/B7.3 evidence must still validate. Before
mutation they require matching all original values, schema1 fingerprint/UUID and
exact rights, reject unexpected objects/stored code/selected-schema sessions,
roles/global/column/table grants and effective PUBLIC writes covering the target.
Unrelated Windows PUBLIC test/test-prefix grants remain accepted.

The exact unchanged SQL is
[rehearsal-migration-grants.sql](../../scripts/phase12/b7/rehearsal-migration-grants.sql),
SHA256 `e86abce5e76d23639a665e2bca51cd1d87bc335451db50122ed505903470e9df`.
It replaces six broad schema privileges with schema SELECT and ten individual
reviewed table privileges (eight table GRANT statements). No DROP/global/grant
option, new account or password change. Migration002 remains
`7de311935843e4fc04cbca74d025d9f38986a4154d70bdd29b35a2820f87b736`;
001 remains `bb901d7580398893b5c04ee263f813b9d05873eb4bae08e0daf377faf73b0ea0`.
Only the runner `--apply --phase12-test --to-version 2` performs DDL.

### Safety and failure behavior

One streamed native administrator session per stage uses Linux sudo/socket or
Windows native password/TCP. The script never captures/caches administrator
passwords; no late root password prompt follows the grant change. Explicit
`--skip-reconnect` prevents transparent native-client reconnection/replay.
Client `--unbuffered` support was inspected using local help, without connecting.
TaskCompass test credentials alone are checked/cached in memory and cleared at exit.

Fresh baseline checks repeat after root authentication and before mutations.
Noclobber attempt markers precede grants/migration; digest-bound success receipts
follow verification. Existing attempts prohibit mutation replay, including uncertain
partial success. Schema2 verification checks all original legacy columns/history,
both migration rows, the completed nine-step journal, empty organization tables,
NULL task ownership, legacy typed payload targets and all59 CHECK/FK health results.
New receipts/exports are sealed beside the original private evidence without
replacing it. No automatic SQL retry/resume/reset/repair/down migration exists.

On interruption, grant failure, DDL failure, connection loss or changed data: stop,
preserve clone and artifacts, report state. MariaDB DDL/grants cannot be assumed to
roll back. Closing a client does not establish whether its SQL executed. Do not
remove markers or rerun a mutation stage; exact-state recovery requires a separate
approval. No development account/database is queried or changed by these scripts.

### Actual agent-executed offline validation

```bash
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure -E '^(MariaDbIntegrationTests|OrganizationIntegrationTests)$'
build/TaskCompassMigrate --print-migration 1 | sha256sum
build/TaskCompassMigrate --print-migration 2 | sha256sum
```

Build succeeded. CTest **19/19 passed**, zero failures, 27.20 seconds; both live
integration suites explicitly excluded. Embedded preview hashes matched unchanged
001/002 above. New offline mocks exercise Linux and Windows branches, exact narrow
rights, complete positive rehearsal, missing approval, wrong SQL/server/session,
data drift, partial/extra grants, partial migration with refused replay, altered
legacy data, nonempty organization tables and nonzero health failures. Existing
B7.2/B7.3 authentication/backup/restore mocks also passed. No live connection,
private backup read, SQL/grant/migration/backup/restore/reset executed by agent.
Native Windows execution of these new scripts remains unverified until owner runs.

During initial offline development, a mocked streamed-root test hung because the
parent retained duplicate pipe descriptors, preventing EOF. Interrupted that fake
run (exit130), closed original descriptors after duplication, and reran successfully.
No live database or private artifacts were involved. Later full suite above passed.

### Operator commands and remaining work

Run one command at a time from the repository root, Linux Bash or Windows UCRT64,
with writers closed and the updated build available:

```bash
scripts/phase12/b74a/01-preflight.sh --approved
scripts/phase12/b74a/02-grants.sh --approved
scripts/phase12/b74a/03-migrate.sh --approved
scripts/phase12/b74a/04-preservation.sh --approved
```

Expected: fresh baseline PASS; narrow grants PASS without migration; one-shot
schema2/UUID verification PASS; original data/history plus CHECK/FK preservation
PASS. The guide specifies account prompts, exact confirmation phrases, new files
and failure policy. Preserve the resulting populated schema2 clones and evidence.
Report actual outputs on both platforms before the separate B7.4b approval request.
No new architectural decision or repeat B7.4a approval is required. No commit/push.

Final documentation checks: changed Markdown local links and fenced blocks passed;
all new/shared shell files passed individual `bash -n`; all20 reviewed-package
checksum entries passed; Phase1.1 milestone and both migration digests remained
unchanged; `git diff --check` passed. No dependencies installed.

## 2026-11-17 — Linux B7.4a populated narrow-role rehearsal passed (owner-reported)

Owner supplied successful terminal output for all four approved commands:

```bash
scripts/phase12/b74a/01-preflight.sh --approved
scripts/phase12/b74a/02-grants.sh --approved
scripts/phase12/b74a/03-migrate.sh --approved
scripts/phase12/b74a/04-preservation.sh --approved
```

Target: `taskcompass_phase12_test`, feledria/MariaDB10.5.29, dataset
`21be9da7-b767-4578-b972-cc9a35122ddb`. These are owner-executed results, not
agent live verification. No failure was reported in this sequence.

- Stage01 verified sealed snapshot/B7.3 receipts, schema1 fingerprint/UUID,
  every original value/raw DDL, server identity and exact broad grants; no
  unexpected authority or selected-schema sessions.
- Stage02 repeated fresh baseline checks, changed only the reviewed clone grants,
  verified exact narrow privileges, and reverified unchanged schema1 data.
  Output: `PASS: exact narrow grant transition verified; original data unchanged. No migration yet.`
- Stage03 revalidated schema1/narrow grants and executed the one-shot migration002.
  Both apply and independent verification reported `Schema 2 verified; dataset
  21be9da7-b767-4578-b972-cc9a35122ddb`.
- Stage04 verified schema2/narrow grants and all original data/history/metadata,
  empty new organizational ownership and zero violations for every CHECK/FK rule.
  All eight sealed new receipt/export files reported OK. Final output:
  `PASS: B7.4a narrow-role schema2 clone; all original data/history/metadata preserved, new owners empty and every CHECK/FK health rule zero.`

Retain the populated Linux schema2 clone and all new B7.4a receipts/exports beside
`/home/justin/temp/TaskCompassBackups/Phase1.2-schema1-linux-20261117-013703`.
Original snapshot digest remains
`83ae094ed33865bba0c22e39174f2977a49cc014de3f669115300105eec2bd26`.
Do not rerun the mutation scripts or disposable integration/reset tests against it.
Windows B7.4a remains approved but unreported/pending; its populated schema1 clone
and exact independently captured snapshot remain preserved pending that rehearsal.
B7.4b runtime-role/probe, B7.5 development upgrade, GUI acceptance and Phase1.3
remain unauthorized. No new approval is requested here.

Reporting-only files modified: this milestone, `docs/Roadmap.md` and
`docs/TestingPhase1.2.md`. Historical entries preserved; current pointers distinguish
Linux schema2 success from pending Windows schema1 rehearsal. No source, SQL,
scripts, private artifacts or databases changed by agent. Application tests/build
not rerun because this update only records owner evidence; prior 19/19 offline
results remain historical. Documentation link/fence checks and `git diff --check`
are performed below as the final reporting checks. No commit/push/merge/tag.

## 2026-11-17 — Windows B7.4a passed; both populated narrow-role rehearsals passed

### Owner-reported execution and results

Owner supplied successful native Windows/UCRT64 output for all four commands:

```bash
scripts/phase12/b74a/01-preflight.sh --approved
scripts/phase12/b74a/02-grants.sh --approved
scripts/phase12/b74a/03-migrate.sh --approved
scripts/phase12/b74a/04-preservation.sh --approved
```

Target was `taskcompass_phase12_test` on feldria-w10/MariaDB13.0.2, dataset
`457bb52b-fea4-4347-9c47-675c6b035c90`. Stage01 rejected the first test-account
password and accepted attempt2/3 using the bounded read-only authentication probe;
no backup/export was attempted by that rejected probe. The remaining stages
completed without reported failure. Password-option notices explicitly reported
unchanged TLS settings; no TLS workaround was applied.

- Preflight validated the exact sealed snapshot/B7.3 receipts, schema1/UUID/
  fingerprint, all original values and raw DDL, native server identity and exact
  original broad grants with no unexpected authority/sessions.
- Grants stage freshly revalidated the baseline, applied the approved narrow
  transition and verified narrow grants plus unchanged schema1 data.
- Migration stage repeated baseline/narrow-role checks, applied migration002 once
  and independently verified schema2. Both runner outputs retained dataset
  `457bb52b-fea4-4347-9c47-675c6b035c90`.
- Preservation stage verified schema2/narrow grants, unchanged original data/
  history/metadata, empty new ownership and zero CHECK/FK health violations.
  All eight new receipt/export files reported OK. Final result:
  `PASS: B7.4a narrow-role schema2 clone; all original data/history/metadata preserved, new owners empty and every CHECK/FK health rule zero.`

Retain the populated Windows schema2 clone and all evidence in
`/c/Users/justin/TaskCompassBackups/Phase1.2-schema1-windows-20261117-064917`.
The original snapshot digest remains
`6588eb9ebc30b63b36b33f2abbc4a0de9da31b252c41a3fd585b8b060073fe50`.
The directory name reflects the original schema1 capture and must not imply that
its now-migrated rehearsal database remains schema1. Preserve all earlier Linux,
Windows and Phase1.1 artifacts and historical results. Do not rerun mutation or
reset tests against either populated schema2 clone.

### Current gate and reporting scope

B7.4a passed on both approved platforms (owner-reported); this is not Phase1.2
acceptance. B7.4b runtime-role/probe is the next separately gated rehearsal;
B7.5 development migration, GUI acceptance and Phase1.3 remain unauthorized.
No new live operation or privilege change is requested or performed here.

Changed only this milestone, `docs/Roadmap.md` and `docs/TestingPhase1.2.md` to
record evidence and update current pointers. Earlier pending statements remain
historical and are superseded by this entry. No code/SQL/scripts, private artifacts,
databases or accounts accessed or modified by agent. Build/CTest not repeated for
this documentation-only report; prior actual offline results remain preserved.
Markdown references/fences and `git diff --check` are the reporting validation.
No commit, push, merge or tag.

## 2026-11-17 — Request to continue to B7.5: prerequisite review

Owner requested continuing to B7.5. Reviewed the B7.4b/B7.5 procedures, exact
rehearsal-runtime grants and rollback-only probe. Both B7.4a migration rehearsals
passed, but no B7.4b runtime-equivalent role/probe result has been reported.
The approved review package explicitly requires both narrow migration and runtime
rehearsals before development upgrade. The request to continue does not explicitly
waive that prerequisite or approve the separately gated clone privilege transition.
No live SQL, grants, migration or private artifact access occurred during this review.
B7.5 development execution is not ready until B7.4b passes on both servers.

### Approval Requested — B7.4b prerequisite, operator execution on both clones

Approve only the existing populated schema2 `taskcompass_phase12_test` clones on
feledria/MariaDB10.5.29 and feldria-w10/MariaDB13.0.2, using the unchanged exact
[rehearsal-runtime-grants.sql](../../scripts/phase12/b7/rehearsal-runtime-grants.sql)
and [runtime-probe.sql](../../scripts/phase12/b7/runtime-probe.sql) documented in
[B7.4b](../Phase1.2B7.md#b74b-runtime-role).

Before the grant transition, freshly verify B7.4a sealed evidence, schema2/UUID,
all legacy data and exact narrow migration privileges. Remove only the reviewed
migration/schema privileges and establish the exact19 individual runtime table
privileges, with no schema/global rights, roles or grant option. Capture all nine
tables, perform Qt read verification and the reviewed rollback-only DML probe,
then require identical before/after nine-table exports, four UPDATE counts1 and
six probe-row counts0. No account/password changes, GUI, development changes,
reset or cleanup. Prepare numbered, offline-mock-tested operator scripts before
execution; agent must not execute live procedures on owner's behalf.

Grant changes are not atomic. Stop on any unexpected result or uncertain/partial
operation; retain the clone and all receipts/exports. No automatic retry, reset,
repair, resume or privilege broadening. The probe uses one new InnoDB transaction
with explicit ROLLBACK and no COMMIT/DDL; disconnect on failure rolls back its
uncommitted DML, but any uncertain outcome requires review and comparison rather
than an assumed pass. Recovery grant changes or restoration require separate
exact-state approval. Preserve Phase1.1 evidence and both development databases.

This entry records the prerequisite and concrete approval scope; it does not
execute or authorize the request on the owner's behalf. Existing exact B7.5 SQL
remains prepared in the review package for later host/artifact-specific approval.
Only the milestone document changed. Markdown references/fences and
`git diff --check` checked; application tests not rerun for documentation-only work.
No commit/push/merge/tag.

## 2026-11-17 — B7.4b approved; runtime rehearsal operator scripts prepared

### Approval decision and current scope

Owner explicitly approved the preceding B7.4b request, both populated schema2
`taskcompass_phase12_test` clones only. This authorizes the exact reviewed
runtime grant transition and rollback-only probe through operator execution.
Source scripts/tests/documentation are prepared below; **live B7.4b not attempted
by agent / pending owner results**. B7.5 development migration, B7.6 runtime dev
changes, GUI writes, recovery/reset and Phase1.3 remain separately gated.
Both successful B7.4a clones and all existing evidence must be preserved.

### Files and implementation

- Added `scripts/phase12/b74b/01-preflight.sh`, `02-runtime-grants.sh`, `03-probe.sh`
  and `run.sh`.
- Extended `scripts/phase12/b74a/run.sh` with definition-only reuse for B7.4b and
  an exact runtime-role metadata policy. Existing B7.4a actions remain unchanged;
  the definition-only path executes no database operation. Reuses pinned platform,
  snapshot/UUID, credential, native administrator, manifest and metadata guards
  rather than duplicating that infrastructure.
- Added `tests/Phase12RuntimeRehearsalScripts.test.sh`; extended existing
  `tests/Phase12MigrationRehearsalScripts.test.sh` fixture-only clients/factory
  with runtime grants, probe results and failure modes. Registered the new test
  in `CMakeLists.txt` using existing Bash/Python dependencies and finite timeout.
- Added [operator guide](../Phase1.2B74b.md), updated current pointers in Roadmap,
  TestingPhase1.2 and this milestone. No production C++/schema/migration/diagram
  or reviewed SQL changes. No dependencies installed.

Before mutation the scripts verify the original pinned B7.2/B7.3 snapshot evidence
and the complete eight-file B7.4a preservation seal. Reject malformed/duplicate/
linked/unexpected manifest paths and incorrect receipt contents. Source and built
migration previews must match001/002. Fresh schema2/UUID verification plus current
legacy data, raw DDL, journal/postconditions and health output must match the sealed
B7.4a outputs; exact server/account/selected database/engine/encoding/session and
privilege metadata checks remain mandatory. No ordinary integration tests run.

Stage01 validates the narrow migration role only. Stage02 captures all nine
schema2 tables before the exact grant transition, revalidates baseline after root
authentication, writes an attempt receipt, executes only the approved grant file,
then checks exact19 runtime table rights/no schema rights, Qt read verification
and unchanged full data before recording success. Stage03 requires the sealed
grant receipts/full baseline; rechecks runtime metadata and data, writes an attempt
receipt before the single probe session, then requires four UPDATE counts1 and six
post-ROLLBACK row counts0. Both nine-table exports must match byte-for-byte, and
schema/legacy/journal/health verification repeats before success/sealing.

Reviewed SQL is unchanged and pinned:

- `rehearsal-runtime-grants.sql`:
  `9af378bc61c18a87bf76bb03f9472fc149ed28ec2914f74eac86ee727937bb7e`.
- `runtime-probe.sql`:
  `0fa9b45a4f14dc9ffb2063915e0d5e5d35f95015ed64beaa2442c36dcc718417`.

Exact runtime matrix: Task/Pillar/Project/Objective SELECT+INSERT+UPDATE;
CommandRecord/LocalChange SELECT+INSERT; Dataset/SchemaMigration/MigrationAttempt
SELECT. No schema/global DML/DDL, DROP/DELETE, roles, grant option, new account,
password/authentication change, or development database access.

### Evidence, failure and recovery

All new files are private and noclobber: `b74b-before-grants.hex.tsv`,
`b74b-grants-attempt.txt`, `b74b-grants-success.txt`, `b74b-grants.sha256`,
`rehearsal-before-probe.hex.tsv`, `rehearsal-after-probe.hex.tsv`,
`b74b-probe-result.tsv`, `b74b-probe-attempt.txt`, `b74b-probe-success.txt`,
`b74b-probe.sha256`. Receipts bind original snapshot, migration and both runtime
SQL digests; attempt markers prevent replay of interrupted/failed operations.
Existing evidence is not replaced. TaskCompass test credentials remain memory-only
with three authentication-only attempts; administrator password stays in the native
client (Linux sudo/socket, Windows native password/TCP). No TLS changes. Native
sessions explicitly disable automatic reconnect and do not use --force.

Grant failure can leave partial rights; stop and preserve state. Probe DML is one
new InnoDB transaction with explicit ROLLBACK/no COMMIT/DDL. Disconnect rolls back
uncommitted DML but does not establish an acceptance result. Unexpected counts,
connection loss or data drift means stop/review, no auto retry/resume/reset/repair,
privilege broadening or restoration. Separate exact-state approval is required for
recovery. Do not rerun disposable tests against these populated clones.

### Actual offline validation and limitations

```bash
TASKCOMPASS_TEST_PYTHON=python3 bash tests/Phase12RuntimeRehearsalScripts.test.sh
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure -E '^(MariaDbIntegrationTests|OrganizationIntegrationTests)$'
sha256sum --check scripts/phase12/b7/ReviewChecksums.sha256
```

Direct new-script mocks passed Linux and Windows positive flows and negative
partial/extra grants, changed baseline, failed probe, wrong counts and nine-table
data drift. Missing approval/help previews do not mutate; mutation replay is
rejected. Failures preserve attempt markers without success receipts. No test
failure was observed in this preparation run.

Build succeeded. Full offline CTest **20/20 passed**, zero failures, 55.25 seconds;
new runtime test28.20 seconds and existing migration rehearsal15.14 seconds.
Both live integration suites explicitly excluded. All20 reviewed-package checksum
entries passed. Phase1.1 milestone and both migration file hashes remained exactly
unchanged. Ordinary tests used fake clients and temporary fixture snapshots only.
Native Windows execution of new scripts and live MariaDB B7.4b are not claimed.
No private backup accessed and no live SQL/grant/probe executed by agent.

### Operator steps and remaining gate

On each platform, from repository-root Bash (Windows UCRT64), close all writers
and run one command at a time, stopping on any error:

```bash
scripts/phase12/b74b/01-preflight.sh --approved
scripts/phase12/b74b/02-runtime-grants.sh --approved
scripts/phase12/b74b/03-probe.sh --approved
```

Enter that platform's `taskcompass_phase12_test` password. Expected outputs:
fresh schema2/narrow baseline PASS; exact19 runtime privileges and unchanged data
PASS; rollback-only probe/all-nine-tables-unchanged PASS. Preserve outputs/clones.
No repeat approval for this exact B7.4b scope is needed. Report both results before
preparing the concrete B7.5 host/snapshot/grant/migration approval checkpoint.
The operator guide contains exact confirmation phrases and recovery limitations.
No commit/push/merge/tag.

Final reporting checks passed: Markdown local references/fenced blocks, individual
`bash -n` on all new/modified shell files, and `git diff --check`. No unresolved
architecture changes or scope conflicts were introduced.

## 2026-11-17 — Linux B7.4b runtime-role rehearsal passed (owner-reported)

### Initial failure and owner service action

Owner's first `01-preflight.sh --approved` validated retained evidence but stopped
at the read-only login/identity probe:

```text
STOP: Login/identity check for taskcompass_phase12_test failed for a reason other than a confirmed password rejection; not retried.
```

No grant or DML stage was reached. Output does not include the native failure code,
so the precise initial connection error is not independently established. Owner
then ran the following OS service commands, after which preflight succeeded:

```bash
sudo systemctl start mariadb
sudo systemctl enable mariadb
```

Enable reported creation of mysql/mysqld/mariadb service symlinks. These were
owner-executed service changes, not agent actions; no account/password/grant repair
or automatic retry occurred. Starting the server resolved the observed inability
to connect. Preserve this initial failed observation rather than treating it as a
password rejection or a successful first attempt.

### Successful owner-executed sequence

```bash
scripts/phase12/b74b/01-preflight.sh --approved
scripts/phase12/b74b/02-runtime-grants.sh --approved
scripts/phase12/b74b/03-probe.sh --approved
```

Target: `taskcompass_phase12_test`, feledria/MariaDB10.5.29, dataset
`21be9da7-b767-4578-b972-cc9a35122ddb`. All subsequent stages passed:

- Preflight validated original B7.2/B7.3 artifacts and B7.4a seal, fresh schema2/
  UUID and exact narrow privileges with no unexpected authority/sessions.
- Runtime transition revalidated the baseline before mutation and verified
  `exact19 runtime table privileges; no schema rights; schema2/data unchanged`.
- Probe verified exact runtime metadata, schema2/UUID and original baseline;
  the scripted result checks and full nine-table before/after equality passed.
  All five final probe exports/receipts reported checksum OK. Final output:
  `PASS: B7.4b runtime verification and rollback-only probe; all nine tables unchanged.`

These are owner-reported live results, not agent-executed queries or independent
artifact inspection. The probe script requires four UPDATE counts1 and six
post-ROLLBACK row counts0 before its reported PASS; individual counts were retained
in the private result file, not displayed in this transcript.

### Current state, remaining work and reporting validation

Linux populated schema2 clone now has the reviewed runtime-equivalent privileges.
Preserve it and all B7.4b receipts/exports in the existing private directory
`/home/justin/temp/TaskCompassBackups/Phase1.2-schema1-linux-20261117-013703`.
Do not rerun its grant/probe stages or disposable reset tests. Windows B7.4b remains
approved but pending owner results. B7.5 still requires both platform rehearsals
and separate concrete development approval; GUI acceptance/Phase1.3 are not started.

Modified only this milestone, Roadmap and TestingPhase1.2 current pointers.
Historical reports preserved. No application/SQL/script/private artifact/database
changes or live operations by agent. No new approval requested in this entry.
Build/tests not repeated for documentation-only reporting; prior 20/20 offline
results remain preserved. Markdown reference/fence and `git diff --check` validation
performed for this update. No commit/push/merge/tag.

## 2026-11-17 — Remove typed confirmation phrases; Windows interruption reviewed

Owner requested no more typed approval phrases or redundant confirmations during
development, keeping password prompts and genuinely required information only.
Recorded this durable operator preference in AGENTS.md. Updated the shared B7.4a/
B7.4b `confirm()` helper to print operation/precondition notices without reading
stdin. Existing --approved flags, fixed target/SQL pins, fresh metadata/data guards,
private evidence checks and no-replay attempt markers remain intact. This change
executes no live procedure and does not implicitly resume partial mutations.

Files changed: AGENTS.md; `scripts/phase12/b74a/run.sh` (shared by B7.4b);
`tests/Phase12MigrationRehearsalScripts.test.sh` and
`tests/Phase12RuntimeRehearsalScripts.test.sh` (now supply passwords only);
`docs/Phase1.2B74a.md`, `docs/Phase1.2B74b.md` and this milestone. Earlier milestone
observations of typed prompts are historical. Earlier standalone B7.2/B7.3 and
Phase1.1 scripts were not refactored as part of this active B7.4 workflow change.

### Owner-reported Windows preflight and Ctrl+C location

Windows B7.4b stage01 passed on feldria-w10/MariaDB13.0.2, dataset
`457bb52b-fea4-4347-9c47-675c6b035c90`, exact narrow migration privileges and
unchanged sealed schema2 evidence. Authentication rejected attempt1 and accepted
attempt2 via the read-only probe. Stage02 was then interrupted at:

```text
Type WRITERS CLOSED; BASELINE UNCHANGED:
```

Inspected stage02 source order: both confirmation reads precede `baseline2`,
`adminStart`, the nine-table export and `b74b-grants-attempt.txt`. Therefore this
reported interruption performed no stage02 live connection, export, receipt write
or grant change; nothing needs database repair or cleanup. Stage01 was read-only.
No claim is made about another unreported attempt.

After updating the repository on Windows, resume at the same stage02 command and
then stage03, with writers closed; no preflight rerun is necessary because stage02
freshly revalidates all baseline/identity/grant prerequisites itself:

```bash
scripts/phase12/b74b/02-runtime-grants.sh --approved
scripts/phase12/b74b/03-probe.sh --approved
```

These updated scripts no longer read approval phrases. Enter only requested
passwords. Existing unexpected attempt markers still stop rather than being deleted
or bypassed. Windows runtime grants/probe remain pending results. Linux B7.4b
success is preserved; do not rerun its mutation stages. No agent live operation,
private artifact access, SQL, schema, migration or backup change occurred.

### Actual validation

```bash
cmake --build build --parallel 2
ctest --test-dir build -R '^Phase12(Migration|Runtime)RehearsalScripts$' --output-on-failure
```

Build reported no work needed. Both relevant offline suites passed (2/2, zero
failures, 42.58 seconds), exercising password-only input on mocked Linux/Windows
flows while preserving approval flags and automatic refusal checks. Markdown
references/fences, shell syntax and `git diff --check` passed. No live/native Windows
run by agent, dependencies installed or commit/push/merge/tag.
