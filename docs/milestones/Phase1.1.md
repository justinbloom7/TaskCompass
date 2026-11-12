<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Phase 1.1 — Persistent Task Foundation

Status: Step B source implementation delivered; live validation pending approval.
Recorded: 2026-11-12. This document saves the proposed implementation plan;
the original Step A plan below is preserved as design history. The subsequent owner
approval authorizes source, migration, test and documentation work, but not live
account/grant changes or migration execution.

## Current status and review map

Permanent repository instructions and milestone reporting are established in
[AGENTS.md](../../AGENTS.md). This file remains the authoritative Phase 1.1
implementation/review record. The original Step A design and dated progress below
are preserved; their historical pending statements are superseded only by the
explicit decisions recorded here.

| Required reporting area | Record / current state |
| --- | --- |
| Status and scope | Step B source delivered; Phase 1.1 incomplete. Deliverable and exclusions below define the boundary. |
| Prerequisites and baseline | Phase 0.1 accepted; owner-reported Linux/Windows results in Baseline and evidence. |
| Plan and architectural decisions | Step A plan preserved below; Step B implementation report records actual choices. |
| Files created/modified | Step B File inventory and subsequent dated entries. |
| Schema and migrations | Initial schema, exact migration DDL and staged provisioning SQL below; no live execution. |
| Tests/commands/results | Executed verification and Final pre-approval verification below; later checks are appended. |
| Issues/limitations/deferred work | Implementation report, exclusions and Outstanding work below; live/Windows Phase 1.1 checks remain unverified. |
| Approvals and decisions | Step A reviewed; Step B source/migrations/tests/docs authorized under owner conditions. Existing Approval Requested section remains pending for live work. |
| Remaining work/completion | Approve and validate isolated integration first; development provisioning/migration is a separate checkpoint, followed by platform/manual acceptance. |

Source authorization does not approve provisioning, grants or migration execution.
This repository-instruction update grants no new database authority or milestone
scope. Subsequent Phase 1.1 work remains within the previously approved plan.

## Baseline and evidence

The owner has completed and accepted Phase 0.1. The owner reports successful builds,
GUI launches and `scripts/dev-database.sh test` on CentOS Stream 9 with MariaDB
10.5.29 and Windows/MSYS2 UCRT64 with MariaDB 13.0.2. These are owner-verified
results, not new agent-executed checks. The isolated taskcompass_dev databases are
reported empty, using the SELECT-only taskcompass_dev@127.0.0.1 account.

Reviewed README, Requirements, Architecture, DataModel, Roadmap, Interactions,
Building, Inspection and Licensing, plus the prototype window, models, dialogs,
diagnostic, build configuration and tests. Historical Phase 0.1 pending statements
are superseded by owner acceptance; retain dated observations as historical evidence.
At the Step A inspection, the code used disposable fixtures and implemented no
persistent tasks; the later Step B report records the subsequent implementation.

Preserve C++20, Qt 6 Widgets, CMake terminal/VIM builds, coding conventions,
MariaDB desktop storage and UI-independent shared logic. No eEnvelopes changes.

## Deliverable and exclusions

Deliver a complete small slice: capture, list, edit, complete, reopen, undo/redo and
restart persistence. Completed tasks are hidden by default and can be shown.
Reopening restores active status and clears the current completion timestamp.

Defer organizational management, contexts, priorities/manual ordering, due and
scheduled dates, recurrence, dependencies, planning, custom fields, mobile,
synchronization and broad GUI styling. Preserve their architectural seams without
adding speculative fields, editors or the full logical schema.

## Proposed classes and boundaries

Names below are proposed; keep responsibilities small during implementation.

| Area | Files / responsibility |
| --- | --- |
| Domain | `src/domain/Task.h/.cpp`: stable identity, minimal fields, lifecycle validation. |
| Application | `src/application/TaskRepository.h`, `TaskService.h/.cpp`, `TaskCommand.h`, `CommandReceipt.h`, `CommandHistory.h/.cpp`: persistence contracts, commands, revision checks and session history. |
| Infrastructure | `src/infrastructure/storage/MariaDbTaskRepository.h/.cpp`, `MigrationRunner.h/.cpp`, connection configuration and injected clock/UUID implementations. |
| Migrations | `migrations/mariadb/001_PersistentTaskFoundation.sql` and a separate migration executable. |
| Desktop | Adapt existing window, table model and dialog; completion, Show Completed, Undo and Redo actions. |
| Tests | Separate pure domain/application, desktop and opt-in MariaDB integration tests. |

Recommend standard C++ value types for domain/application interfaces, independent
of Widgets, QML and Qt SQL. Perform Qt conversions at adapter boundaries. Inject
clock and ID generation for deterministic tests. Use a serialized database worker
with its connection owned by that thread; refresh GUI state after successful commits.

## Initial schema

Implement only the following InnoDB tables using utf8mb4, prepared parameters,
canonical UUID strings, UTC timestamps with millisecond precision and constraints.

| Table | Initial purpose |
| --- | --- |
| Dataset | One stable local dataset identity. |
| Task | UUID, dataset UUID, title, optional notes, active/completed status, createdAt, updatedAt, optional completedAt, localRevision and deletion tombstone. |
| CommandRecord | Immutable receipts with affected task, before/after snapshots, revisions and compensation/reapplication links. |
| LocalChange | Dormant transactional change records; no networking or sync engine. |
| SchemaMigration | Version, checksum and application timestamp. |

Enforce nonblank titles, supported status values, completed status iff completedAt
is present, positive revisions and dataset-consistent references. Completion
history initially resides in CommandRecord; work recording remains deferred.
Preserve PascalCase table names and exact query spelling. Validate case behavior
and schema portability on Linux and Windows before freezing the migration.

## Controlled migration strategy

1. Run migrations explicitly with the separate migration executable; GUI/runtime
   credentials never automatically migrate a schema.
2. Acquire a database-scoped migration lock and inspect versions/checksums.
3. Reject newer, inconsistent or unsupported schemas.
4. Apply numbered migrations deterministically and verify the resulting schema.
5. Record success only after verification.

MariaDB DDL can implicitly commit. A partial migration stops with actionable
recovery instructions; never claim it rolled back or silently mark it successful.
Migration 001 contains no drops or destructive alterations. Specify and test exact
partial-initialization detection and recovery before executing it. Backup/restore
procedures are part of validation, not an ordinary undo operation.

## Proposed privilege separation — approval required

Keep the existing SELECT-only diagnostic account unchanged. Provisioning is an
explicit administrator operation, separate from application or migration execution.
Neither executable creates users or modifies grants.

| Account | Proposed minimum privileges |
| --- | --- |
| Existing diagnostic account | Existing SELECT-only access. Its empty-schema validation remains a Phase 0.1 diagnostic and is not the post-migration test procedure. |
| Dedicated runtime account | SELECT/INSERT/UPDATE on the task, command and change tables as needed; SELECT on dataset and migration metadata. No DELETE, DDL or grant authority. |
| Dedicated migration account | For migration 001: schema-scoped CREATE and the SELECT/INSERT needed for metadata initialization/verification. No global privileges, DROP or GRANT OPTION. |

Choose account names and review exact grants with the owner before provisioning.
Later migrations request additional privileges individually. Establish the existing
environment-based configuration mechanism for each executable; no credentials in
source, build artifacts, logs or Git. Never access unrelated databases.

Approval of the design permits Step B implementation. Live account provisioning
and migration execution require review/approval of the concrete commands and
authorized credentials; do not infer permission to silently elevate access.

## Commands and undo/redo

Create, edit, complete and reopen are explicit application commands. Each validates
input and expected revision, then commits task changes, durable receipt and dormant
change record in one transaction. Failed commands never enter session history.

Use a small commit-gated session history controller, as permitted by Architecture,
rather than persistence inside QUndoStack callbacks. Undo/redo execute new validated
compensating commands. Restore exact prior business-field values while advancing
revision and modification metadata; never erase durable history. Undo creation
tombstones the task and redo restores its stable UUID. Reject stale preconditions
rather than overwriting newer changes. Future dependent records must block unsafe
creation inverses when those features arrive.

Initially retain up to 100 session commands. Successful new edits discard the redo
branch; failed/cancelled edits do not. Failed undo/redo leaves the history index
unchanged. Session history clears on restart; persisted audit history remains but
does not reconstruct an undo stack. Long-term history recovery is separate.

## Desktop integration

Preserve menus, toolbar, splitter, navigation, table and dialog interaction patterns.
Show real independent tasks without presenting fixture organizational nodes as
actual ownership. Keep the fixture prototype available as a separate demonstration/
test path. Decide the smallest code-sharing arrangement while implementing; avoid
duplicating business logic or undertaking a general window redesign.

Support title/notes edits, lifecycle-derived completion checkboxes, Show Completed,
explicit reopening, descriptive Undo/Redo and stable selection by UUID after
refreshes. Unsupported priority/date/ownership fields are not editable. Display
validation/database failures clearly, preserving committed state and keeping the
GUI responsive. No credentials are saved in layout settings.

## Tests and completion criteria

| Layer | Required validation |
| --- | --- |
| Pure domain/application | Invalid inputs, creation/editing, lifecycle and timestamps, all undo/redo operations, stale revisions, failed commands and redo invalidation; no live server. |
| MariaDB integration | Explicit separate test database; migrations/version/checksum handling, partial initialization, incompatible versions, constraints, restart/reload persistence and atomic rollback including failure between task/history writes. Never select a database implicitly or destructively reset taskcompass_dev. |
| Desktop | Capture/edit/cancel, completion/reopen, filtering, history actions, stable selection and database error feedback. |
| Platforms/manual | CentOS build/CTest/live MariaDB; Windows UCRT64 build/CTest/live MariaDB 13; interactive capture/edit/undo/restart/reopen and backup/restore. |

Ordinary CTest must not require a server. Integration tests are explicit opt-in and
require a validated isolated database/account. Document any necessary test-only DDL
privileges separately; do not broaden runtime privileges to support test cleanup.

Complete Phase 1.1 only when the persistent workflows, transaction/history
invariants and required platform validation pass. Record actual commands/results
here, distinguishing owner reports, agent-executed checks and pending tests.

## Approval and execution record — original Step A snapshot

- Step A: plan presented and saved; owner approval for Step B pending.
- Step B: not started. No production source, migrations or accounts created.
- Step C: not started. No Phase 1.1 build/integration validation claimed.
- Step D: append implementation results, limitations and remaining work here.

Approval items: minimal schema/privileges, standard-C++ shared types, separate
migration executable, commit-gated history controller and session-only undo.
Do not commit, merge, tag or push without explicit instruction.


## Step B authorization and initial progress — 2026-11-12

Owner approved the architecture and Step B source implementation, with explicit
checkpoints before provisioning, grants or live migrations. Milestone reporting
must remain in this document. No Git commits/pushes or unrelated database access.

Implemented so far: standard-C++ task rules and command/history service; Qt SQL
connection, transaction adapter, embedded numbered migration and explicit migration
CLI; separate persistent desktop preserving the prototype; pure and desktop tests;
compiled opt-in MariaDB integration tests. Detailed file inventory, final results
and concrete SQL/recovery approval request will follow below before stopping.

Initial verification: Linux CMake build succeeded; six database-free CTest targets
passed and the live integration target was skipped. No live database connection,
provisioning, migration or permission change was attempted in Step B. Source review
identified ambiguous commit handling; the desktop now invalidates session history
and requires reload/inspection if commit success cannot be confirmed. Tests are
being extended before the final checkpoint. Phase 1.1 is not complete.


## Step B implementation report — pre-database checkpoint

### Completed source work and decisions

- `TaskCore` is standard C++20, with no Qt dependency. Explicit TaskStatus and
  TaskCommand types validate title/notes, canonical UUIDs, lifecycle timestamps,
  revision limits and stale state. Clock/UUID generation are injected.
- `TaskService` owns commands and the commit-gated chronological history (100
  commands). Multiple edits/undos of the same task track the latest session-owned
  revision, so undoing an earlier command does not incorrectly reuse an old revision.
  If a subsequent accepted edit observes an external revision on a session-known
  task, it invalidates older session history after commit so stale inverses cannot
  become accidentally valid again. Without a new edit, stale undo is rejected.
  Edits restore only title/notes; lifecycle inverses restore exact prior status and
  completedAt. Undo-create tombstones; redo restores the same UUID. Revisions and
  updatedAt advance; createdAt stays unchanged. Descriptive Undo/Redo labels are exposed.
- `MariaDbTaskRepository` uses bound parameters and revision-guarded writes. Task,
  immutable CommandRecord and dormant LocalChange commit together. Failure at task,
  audit or change write rolls back the transaction. Receipt snapshots are versioned
  JSON with 64-bit integers encoded as decimal strings. No automatic retry or sync.
- `DatabaseConnection` sets utf8mb4, UTC, strict SQL mode, constraint checking and
  stable SHOW CREATE quoting. Errors identify the failing stage without printing
  connector text that could disclose credentials. Connection failures do not diagnose
  a wrong password specifically; stopped service, plugin and account/grants remain
  possible causes. No passwords are written by the code or these checks.
- `TaskCompassMigrate` defaults to printing SQL, without environment inspection or
  connection. `--verify` checks the exact table set, version/checksum/fingerprint and
  one dataset. `--apply` requires the dedicated migration/test account and uses a
  migration lock; the desktop never calls it. Partial DDL never resumes automatically.
- `TaskCompass` is a separate persistent desktop executable, leaving the entire
  accepted prototype and its tests/scripts unchanged. The same traditional window
  pattern is used with an independent-task navigation node. No fake organizational
  ownership, priority or due-date editors appear in the persistent application.
- The database worker owns the connection/repository/service; GUI edits serialize,
  refresh from committed results, and preserve selection by UUID when still visible.
  Completion checkboxes submit commands and wait for commit. Show Completed defaults
  off; reopening clears completedAt. Closing during an operation is deferred.
- Known command failures preserve history and the last visible snapshot; editing
  pauses until View → Reload. A lost/uncertain COMMIT acknowledgment is different:
  outcome may already be durable, so session history is explicitly invalidated and
  the user must reload/inspect before retrying. The UI does not retry blindly.
- Implementation choices: declarations for TaskCommand/CommandReceipt are separate
  headers; the small history controller stays inside TaskService rather than a
  separate CommandHistory class. The focused editor is local to the persistent
  window. Qt widgets/SQL never cross the domain/application interfaces.

Physical representation decisions: UTC instants are signed BIGINT epoch milliseconds,
not server-local DATETIME; title limit is 1024 UTF-8 bytes and optional notes 65535
bytes. Both reject invalid UTF-8/NUL. Trimming uses ASCII whitespace. These explicit
initial limits avoid a Unicode dependency in the core. UUIDv4 generation uses Qt
only in composition. Future ownership/context/priority/date columns remain deferred.

Runtime/task API intentionally accepts only loopback taskcompass_dev with
`taskcompass_app`; migration apply uses `taskcompass_migrate`; integration uses
only taskcompass_integration/taskcompass_test. Root, diagnostic accounts, remote
hosts and unrelated database names are rejected before connecting. This is a
Phase 1.1 safety boundary, not a finalized production configuration system.

Schema fingerprinting adds one metadata field beyond the initial proposal. It
hashes normalized SHOW CREATE output for the five tables; data writes do not affect
it. Server upgrades may reformat that output and cause conservative rejection.
Do not edit fingerprints to bypass verification; a reviewed upgrade/migration is
needed if formatting changes. Cross-version dump/restore portability is unverified.

### File inventory

Modified: `CMakeLists.txt`, `README.md`, `docs/Requirements.md`,
`docs/Architecture.md`, `docs/DataModel.md`, `docs/Roadmap.md`, `docs/Building.md`
and this milestone. No prototype source, existing tests, diagnostic or shell
helper source was modified.

Created:

- `src/domain/Task.h`, `Task.cpp`.
- `src/application/TaskCommand.h`, `CommandReceipt.h`, `CommitOutcomeUnknown.h`,
  `TaskRepository.h`, `TaskService.h`, `TaskService.cpp`.
- `src/infrastructure/storage/DatabaseConfig.h/.cpp`, `DatabaseConnection.h/.cpp`,
  `MariaDbTaskRepository.h/.cpp`, `MigrationRunner.h/.cpp`, `MigrateMain.cpp`,
  `Migrations.qrc`.
- `migrations/mariadb/001_PersistentTaskFoundation.sql`.
- `src/desktop/persistent/PersistentTaskSession.h`,
  `PersistentTaskController.h/.cpp`, `PersistentTaskModel.h/.cpp`,
  `PersistentTaskFilter.h/.cpp`, `PersistentTaskWindow.h/.cpp`, `PersistentMain.cpp`.
- `tests/MemoryTaskRepository.h`, `TaskCoreTests.cpp`,
  `PersistentDesktopTests.cpp`, `StorageSafetyTests.cpp`, `MariaDbIntegrationTests.cpp`.

### Executed verification and results

All checks below were database-free. No server SQL, provisioning, grants or live
migration was executed. No packages installed or Git commit/merge/tag/push performed.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

Observed CentOS packages: GCC/G++ 11.5.0, Qt development/QMYSQL 6.10.3, MariaDB server
package 10.5.29; CMake 3.31.8. Package presence is not a new server connection result.
Incremental builds succeeded. Initial Qt deprecation warnings were corrected using
QTimeZone and version-gated filter APIs; subsequent changed-target builds were clean.

| Target | Actual result / scope |
| --- | --- |
| PrototypeTests | Passed; original fixture/model/dialog/action/layout tests. |
| GuiStartup | Passed offscreen; original accepted prototype. |
| DevDatabaseScripts | Passed mocked-client suite; no live connection. |
| TaskCoreTests | Passed: invalid inputs/UTF-8, creation/editing, completion/reopen timestamps, all inverse/redo operations, failed commands/history, stale revisions, redo branching, restart history policy, 100-command cap. In-memory repository, not MariaDB proof. |
| MigrationSqlPreview | Passed; SQL printed without connecting. |
| PersistentDesktopTests | Passed offscreen: Qt model contracts, checkbox commit gate, worker failure/reload, uncertain commit/history invalidation, dialog editing/cancel/capture, selection through sorting, completion/reopen/filter/undo/redo. Fake repository, not live persistence. |
| StorageSafetyTests | Passed: account/database/host/port/explicit-isolation guards and embedded migration equals source. No database connection. |
| MariaDbIntegrationTests | **Skipped** by default, exit 77 before reading configuration or connecting. Compiled, not run live. |

No test failures observed in executed runs. Seven tests passed; one live test
skipped. This is not acceptance of the SQL or live task persistence. Windows builds
of Phase 1.1, MariaDB 10.5/13 integration, actual desktop restart persistence,
privilege-negative testing, backup/restore and interactive usability remain unverified.

The integration executable requires `--run --allow-reset`, explicit TEST environment,
exact database/user and CURRENT_USER matching. It rejects unexpected table names
before reset. It exercises partial initialization, migration idempotence/version/
checksum/fingerprint rejection, UTF-8 SQL round trips, real reconnection, all commands,
revision conflicts, and rollback after task/audit/change failures. It intentionally
resets only its five known tables in taskcompass_integration, never taskcompass_dev.
On failure it leaves evidence and does not automatically clean up. The test-only
account has broader rights than runtime solely for these isolated fault scenarios.

## Approval Requested — staged live validation

**Pending owner approval; none of this section has been executed.** Request approval
first for **Stage 1 only**: read-only preflight, create the isolated integration
database/test account below, and execute the compiled integration tests on Linux.
That approval would cover the test's explicit destructive reset/DDL/DML inside
**taskcompass_integration only**. Review its SQL file and test source alongside this
report. Stop and report results before Stage 2 development provisioning/migration.
Windows execution requires an owner-operated Windows/UCRT64 machine and separate
reported results. No approval is inferred from elapsed time or previous Step B approval.

### Stage 1 — exact integration provisioning SQL

Use an authorized administrator for provisioning only. Inspect the two named schemas
and the three new account names first; never print authentication hashes/passwords.
Read-only preflight (restricted to requested targets):

```sql
SELECT VERSION();
SELECT SCHEMA_NAME, DEFAULT_CHARACTER_SET_NAME, DEFAULT_COLLATION_NAME
   FROM information_schema.SCHEMATA
   WHERE SCHEMA_NAME IN ('taskcompass_dev', 'taskcompass_integration');
SELECT TABLE_SCHEMA, TABLE_NAME FROM information_schema.TABLES
   WHERE TABLE_SCHEMA IN ('taskcompass_dev', 'taskcompass_integration');
SELECT User, Host FROM mysql.user
   WHERE User IN ('taskcompass_app', 'taskcompass_migrate', 'taskcompass_test');
```

If integration database or any proposed account already exists, stop and inspect;
do not overwrite passwords/grants or assume the existing contents are disposable.
Existing-role/PUBLIC grants that cover either target also need administrator review
without changing them automatically. Keep the diagnostic account untouched.

For confirmed absent resources only, after approval:

```sql
CREATE DATABASE taskcompass_integration
   CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER 'taskcompass_test'@'127.0.0.1'
   IDENTIFIED BY '<distinct-strong-test-password>';
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, DROP, ALTER
   ON `taskcompass\_integration`.*
   TO 'taskcompass_test'@'127.0.0.1';
```

Password placeholders are not literal passwords. Use privately generated/saved
credentials, with account SQL supplied through a reviewed secret-safe stdin path;
do not type real password SQL in shell history, log it, or save it in the repo.
Review server-side general/audit logging before provisioning secrets. No global
privileges, GRANT OPTION or application use of root are requested.

Once approved/provisioned, the test environment and command (Linux):

```bash
export TASKCOMPASS_TEST_DB_HOST=127.0.0.1 TASKCOMPASS_TEST_DB_PORT=3306
export TASKCOMPASS_TEST_DB_NAME=taskcompass_integration
export TASKCOMPASS_TEST_DB_USER=taskcompass_test TASKCOMPASS_TEST_DB_ISOLATED=1
read -r -s -p 'Integration database password: ' TASKCOMPASS_TEST_DB_PASSWORD
export TASKCOMPASS_TEST_DB_PASSWORD
./build/MariaDbIntegrationTests --run --allow-reset
unset TASKCOMPASS_TEST_DB_PASSWORD
```

Do not execute yet. Windows/UCRT64 uses `build-ucrt64/MariaDbIntegrationTests.exe`.
The default CTest invocation stays non-networked. Optional registration of the live
command requires explicit `-DTASKCOMPASS_ENABLE_DB_TESTS=ON`; keep that OFF until
approved and return it to OFF after live work. The direct executable is preferable
for this first approval checkpoint to avoid accidentally rerunning destructive tests.

Exact reset DDL used by the test, only after checking database/current-user and
rejecting unknown table names:

```sql
DROP TABLE IF EXISTS LocalChange;
DROP TABLE IF EXISTS CommandRecord;
DROP TABLE IF EXISTS Task;
DROP TABLE IF EXISTS Dataset;
DROP TABLE IF EXISTS SchemaMigration;
```

The test applies migration 001 below, intentionally creates only the first table
once to simulate partial DDL, verifies refusal, resets, and initializes again. It
also temporarily sets SchemaMigration.version=99 and checksum=REPEAT('0',64), and
executes `ALTER TABLE Task ADD COLUMN unexpectedTestColumn INT NULL` to verify drift
rejection. Task/receipt/change DML and rollback fault setup use prepared statements
in `tests/MariaDbIntegrationTests.cpp`. A successful run leaves this test schema
empty. A failed run retains evidence for operator inspection.

### Stage 2 — development provisioning SQL (future separate approval)

**Not requested for execution until Stage 1 results have been reviewed.** Existing
`taskcompass_dev` and its SELECT-only account/password/grants remain untouched.
The new account names below are proposed concrete role separation, implemented as
separate accounts rather than inherited MariaDB roles.

Before migration, after confirming these accounts do not exist:

```sql
CREATE USER 'taskcompass_migrate'@'127.0.0.1'
   IDENTIFIED BY '<distinct-strong-migration-password>';
GRANT CREATE, SELECT ON `taskcompass\_dev`.*
   TO 'taskcompass_migrate'@'127.0.0.1';
GRANT CREATE, INSERT ON taskcompass_dev.Dataset
   TO 'taskcompass_migrate'@'127.0.0.1';
GRANT CREATE, INSERT ON taskcompass_dev.SchemaMigration
   TO 'taskcompass_migrate'@'127.0.0.1';
```

The table-level CREATE grants are redundant with schema CREATE but permit granting
metadata INSERT before the tables exist. No migration UPDATE/DELETE/ALTER/DROP,
global or grant authority is requested for migration 001. Verify this minimal grant
sequence on the supported servers before treating it as validated.

After migration and schema verification, provision runtime table grants:

```sql
CREATE USER 'taskcompass_app'@'127.0.0.1'
   IDENTIFIED BY '<distinct-strong-runtime-password>';
GRANT SELECT, INSERT, UPDATE ON taskcompass_dev.Task
   TO 'taskcompass_app'@'127.0.0.1';
GRANT SELECT, INSERT ON taskcompass_dev.CommandRecord
   TO 'taskcompass_app'@'127.0.0.1';
GRANT SELECT, INSERT ON taskcompass_dev.LocalChange
   TO 'taskcompass_app'@'127.0.0.1';
GRANT SELECT ON taskcompass_dev.Dataset
   TO 'taskcompass_app'@'127.0.0.1';
GRANT SELECT ON taskcompass_dev.SchemaMigration
   TO 'taskcompass_app'@'127.0.0.1';
```

Runtime cannot update/delete durable audit/change records or mutate dataset/schema
metadata. Tombstones use Task UPDATE; physical DELETE is unnecessary. After initial
migration, the operator can remove migrator write privileges (also subject to Stage 2
approval) and retain read-only verification access:

```sql
REVOKE CREATE ON `taskcompass\_dev`.* FROM 'taskcompass_migrate'@'127.0.0.1';
REVOKE CREATE, INSERT ON taskcompass_dev.Dataset FROM 'taskcompass_migrate'@'127.0.0.1';
REVOKE CREATE, INSERT ON taskcompass_dev.SchemaMigration FROM 'taskcompass_migrate'@'127.0.0.1';
```

Schema-level escaped underscores restrict wildcard grants to the exact database.
Table-level names are literal. CREATE covers inline indexes; SHOW CREATE requires
SELECT. See [MariaDB GRANT](https://mariadb.com/docs/server/reference/sql-statements/account-management-sql-statements/grant)
and [SHOW CREATE TABLE](https://github.com/mariadb-corporation/mariadb-docs/blob/main/server/reference/sql-statements/administrative-sql-statements/show/show-create-table.md).

### Exact migration 001 DDL — proposed, never executed live

Authoritative executable file: [001_PersistentTaskFoundation.sql](../../migrations/mariadb/001_PersistentTaskFoundation.sql).
The embedded resource must byte-match that file (automated test). Its SHA-256:

`bb901d7580398893b5c04ee263f813b9d05873eb4bae08e0daf377faf73b0ea0`

```sql
CREATE TABLE SchemaMigration (
   version INT NOT NULL PRIMARY KEY,
   checksum CHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   schemaFingerprint CHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   appliedAt BIGINT NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE Dataset (
   id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL PRIMARY KEY,
   name VARCHAR(255) NOT NULL,
   createdAt BIGINT NOT NULL,
   CONSTRAINT datasetIdFormat CHECK (id REGEXP '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$'),
   CONSTRAINT datasetCreatedAt CHECK (createdAt > 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE Task (
   id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL PRIMARY KEY,
   datasetId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   title VARCHAR(1024) NOT NULL,
   notes MEDIUMTEXT NULL,
   lifecycleStatus VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   createdAt BIGINT NOT NULL,
   updatedAt BIGINT NOT NULL,
   completedAt BIGINT NULL,
   deletedAt BIGINT NULL,
   localRevision BIGINT NOT NULL,
   CONSTRAINT taskDataset FOREIGN KEY (datasetId) REFERENCES Dataset(id),
   CONSTRAINT taskIdFormat CHECK (id REGEXP '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$'),
   CONSTRAINT taskTitle CHECK (OCTET_LENGTH(title) BETWEEN 1 AND 1024 AND CHAR_LENGTH(TRIM(title)) > 0),
   CONSTRAINT taskNotes CHECK (notes IS NULL OR OCTET_LENGTH(notes) <= 65535),
   CONSTRAINT taskLifecycle CHECK ((lifecycleStatus = 'active' AND completedAt IS NULL) OR (lifecycleStatus = 'completed' AND completedAt IS NOT NULL)),
   CONSTRAINT taskTimes CHECK (createdAt > 0 AND updatedAt >= createdAt AND (completedAt IS NULL OR completedAt BETWEEN createdAt AND updatedAt) AND (deletedAt IS NULL OR deletedAt BETWEEN createdAt AND updatedAt)),
   CONSTRAINT taskRevision CHECK (localRevision > 0),
   INDEX taskActive (datasetId, deletedAt, lifecycleStatus),
   UNIQUE KEY taskDatasetIdentity (datasetId, id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE CommandRecord (
   id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL PRIMARY KEY,
   operationId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
   datasetId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   taskId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   commandType VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   payloadVersion INT NOT NULL,
   recordedAt BIGINT NOT NULL,
   beforeSnapshot MEDIUMTEXT NULL,
   afterSnapshot MEDIUMTEXT NOT NULL,
   resultingRevision BIGINT NOT NULL,
   compensatesCommandId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL,
   reappliesCommandId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL,
   CONSTRAINT commandTask FOREIGN KEY (datasetId, taskId) REFERENCES Task(datasetId, id),
   CONSTRAINT commandPayload CHECK (payloadVersion = 1 AND resultingRevision > 0 AND JSON_VALID(afterSnapshot) AND (beforeSnapshot IS NULL OR JSON_VALID(beforeSnapshot))),
   CONSTRAINT commandKind CHECK (commandType IN ('create', 'edit', 'complete', 'reopen', 'undo', 'redo')),
   INDEX commandTaskHistory (datasetId, taskId, resultingRevision)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE LocalChange (
   id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL PRIMARY KEY,
   operationId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
   datasetId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   taskId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   localRevision BIGINT NOT NULL,
   payloadVersion INT NOT NULL,
   immutablePayload MEDIUMTEXT NOT NULL,
   createdAt BIGINT NOT NULL,
   acknowledgmentState VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'pending',
   CONSTRAINT changeTask FOREIGN KEY (datasetId, taskId) REFERENCES Task(datasetId, id),
   CONSTRAINT changeCommand FOREIGN KEY (operationId) REFERENCES CommandRecord(operationId),
   CONSTRAINT changePayload CHECK (payloadVersion = 1 AND localRevision > 0 AND JSON_VALID(immutablePayload)),
   CONSTRAINT changeState CHECK (acknowledgmentState = 'pending'),
   INDEX changePending (datasetId, acknowledgmentState, createdAt)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
```

After the five CREATE statements, the tool calculates the schema fingerprint and
runs this prepared metadata transaction, with no task/application records seeded:

```sql
START TRANSACTION;
INSERT INTO Dataset (id, name, createdAt) VALUES (?, ?, ?);
INSERT INTO SchemaMigration (version, checksum, schemaFingerprint, appliedAt)
   VALUES (1, ?, ?, ?);
COMMIT;
```

Bindings: generated canonical UUIDv4; `Local tasks`; current UTC epoch milliseconds;
SHA-256 of the exact DDL bytes; SHA-256 of normalized SHOW CREATE output in fixed
SchemaMigration/Dataset/Task/CommandRecord/LocalChange order; current UTC milliseconds.
Read-only queries inspect only the selected schema, its five tables and dataset.
GET_LOCK/RELEASE_LOCK use `CONCAT('TaskCompassMigration:', DATABASE())` on that
connection; no global table lock or extra schema is used.

### Partial failure, backup and recovery procedure

Before a development migration, stop TaskCompass and any writers. Verify the exact
server/database identity, expected empty tables, charset/collation, account grants
and absence of conflicting roles/PUBLIC write privileges. Preserve a private backup
outside Git, including the empty baseline. Use an existing authorized backup account;
do not grant backup capabilities to runtime. Example operator command (not executed):

```bash
mariadb-dump --no-defaults --protocol=TCP --host=127.0.0.1 --port=3306 \
   --user='<authorized-backup-account>' --password --single-transaction \
   --skip-lock-tables --skip-add-drop-table --databases taskcompass_dev \
   --result-file='<private-backup-path>/taskcompass_dev-before-phase1.1.sql'
```

Protect the dump as user data, verify successful exit and inspect that it targets
only taskcompass_dev. Record its checksum and test restoration to an explicitly
approved scratch target/instance before relying on it. Do not load a database-scoped
dump into an unrelated schema by accident; database names in dumps matter. Backup
and restore execution, scratch provisioning and any destructive restore require
reviewed target-specific approval. No backup/restore has been attempted yet.

Migration detection/recovery:

1. With no tables, apply may initialize. With existing tables it only verifies a
   fully completed migration; any missing/extra table, absent migration row,
   mismatched checksum/fingerprint, unsupported version or missing/multiple dataset
   stops. No `IF NOT EXISTS` DDL silently adopts existing objects.
2. CREATE statements may have committed even if a later statement fails. The
   metadata transaction rolls back on known failure, leaving DDL for inspection.
   An uncertain metadata commit must be inspected, not assumed rolled back.
3. Preserve error output (no credentials), list current target tables and migration
   metadata, and compare SHOW CREATE with the reviewed migration. Do not manually
   insert a successful version row, edit checksums, or rerun individual statements
   to hide a partial initialization.
4. For this first migration on an independently confirmed empty baseline, either
   restore the verified backup or request separate approval to remove only the five
   partial tables in reverse dependency order (the DROP list above), **explicitly
   qualified for taskcompass_dev in any approved recovery command**. The runtime
   and migrator cannot perform that cleanup. Do not execute the integration-reset
   routine on development; it rejects that target by design.
5. If any data exists or the baseline is uncertain, stop. Preserve data and plan a
   reviewed forward repair or backup restoration. There is no automatic down
   migration, live-schema rollback or physical purge in this milestone.

MariaDB [DDL implicit commits](https://mariadb.com/docs/server/reference/sql-statements/transactions/sql-statements-that-cause-an-implicit-commit)
prevent claiming transactional schema rollback. Ordinary task writes remain
transactional. A server format change may trigger fingerprint rejection after an
upgrade/restore; resolve through a reviewed migration/compatibility procedure,
never automatic metadata rewriting.

After Stage 2 approval, the migration would use hidden-prompt environment loading,
override USER to taskcompass_migrate, then invoke `TaskCompassMigrate --apply` and
`--verify`; clear the password afterward. The desktop uses taskcompass_app and
never --apply. Linux uses build; Windows/UCRT64 uses build-ucrt64 and `.exe`.
The existing dev-database.sh empty-schema test will intentionally reject development
after migration; it and its diagnostic account remain unchanged.

### Outstanding work and acceptance status

Completed: source implementation, migration SQL/resource/CLI, database-free tests
and detailed review materials. **Pending approval:** Stage 1 integration provisioning
and live test DDL/DML. **Not attempted:** any live Phase 1.1 SQL, grant changes,
development migration, Windows Phase 1.1 build/tests, real restart persistence,
interactive persistent desktop review and backup/restore verification.

After Stage 1, append actual Linux/MariaDB results here and fix any incompatibilities.
Request the Stage 2 development grant/migration approval separately; validate minimal
runtime rights and clear DDL/write-denial behavior without destructive tests.
Windows/MariaDB 13 results must be recorded independently. Confirm backup/restore
and actual desktop capture/edit/completion/reopen/undo/restart behavior before
closing Phase 1.1. No later-phase work is authorized.

No owner-facing behavior decision remains newly blocked before Stage 1; the explicit
byte limits, fingerprint conservatism, account names and fixed development/test
configuration are implementation choices exposed for review. Live SQL compatibility
and deployment procedures remain unverified, not assumed. No commits or pushes.


### Final pre-approval verification

After the last code review (including external-revision history invalidation),
`cmake --build build --parallel 2` and `ctest --test-dir build --output-on-failure`
again succeeded: seven passed, MariaDbIntegrationTests skipped, zero failures.
`git diff --check` passed. Local Markdown links/fences and the inline migration
SQL/SHA-256 were checked against the source and passed. `ldd build/TaskCoreTests`
shows standard C++/system runtimes only, confirming no Qt linkage for the core test.
A source diff confirmed the original prototype, original tests and shell helpers
are unchanged. No live SQL was attempted. Current checkpoint: awaiting Stage 1
approval as requested above; Stage 2 remains a later separate approval.

## Repository instructions and reporting update — 2026-11-12

Completed the owner's documentation-only instruction task. Created root
`AGENTS.md` with permanent architecture/conventions, milestone records, reporting,
approval, database-safety and evidence-based validation policies. These apply
immediately and to subsequent milestones; milestone-specific authority stays here.

Files created/modified by this step:

- Created [AGENTS.md](../../AGENTS.md).
- Updated [README.md](../../README.md) with one short reference to the permanent
  instructions; did not duplicate their complete contents elsewhere.
- Updated this milestone with a nine-area current-status/review map, clarified the
  original Step A baseline, and appended this report. Preserved the original plan,
  implementation decisions, results, SQL and pending Approval Requested section.
- Updated [Roadmap.md](../Roadmap.md) to label its old Phase 0.2 next-step
  recommendation as historical and point to the current approval checkpoint.

Approval/status confirmation: Phase 0.1 has been accepted; the owner reviewed
Phase 1.1 Step A and authorized Step B source/migration/test/documentation work
under the previously recorded conditions. Live account provisioning, grants and
migration execution still require separate explicit approval. Establishing
AGENTS.md does not satisfy or broaden that pending authority. No production
source, SQL file, system package, account or database was changed in this step.
Earlier uncommitted implementation files remain in the working tree; they are not
new changes from this documentation task. No commit, merge, tag or push performed.

### Consistency review

No substantive conflict was found between AGENTS.md and the approved architecture
or milestone conditions. Its prohibition on automatic database resets does not
approve the proposed opt-in integration reset: that action still needs the explicit
isolated-target/procedure approval requested above. The old roadmap next-step
paragraph could be mistaken for a current source-implementation prohibition; it
is now explicitly historical. Other dated Phase 0.1 pending observations remain
historical evidence, superseded by owner acceptance rather than silently rewritten.
The current review map distinguishes Step A's old approval-pending snapshot from
the subsequent actual Step B authorization.

### Validation executed

```bash
python3 - <<'PY'
from pathlib import Path
import re
paths = ['AGENTS.md', 'README.md', 'docs/Roadmap.md', 'docs/milestones/Phase1.1.md']
for name in paths:
    path = Path(name)
    content = path.read_text()
    assert sum(line.startswith('```') for line in content.splitlines()) % 2 == 0, name
    for target in re.findall(r'\]\(([^)]+)\)', content):
        if '://' not in target and not target.startswith('#'):
            assert (path.parent / target.split('#')[0]).exists(), (name, target)
report = Path('docs/milestones/Phase1.1.md').read_text()
assert '## Approval Requested — staged live validation' in report
assert '## Approval and execution record — original Step A snapshot' in report
assert Path('migrations/mariadb/001_PersistentTaskFoundation.sql').read_text() in report
print('PASS: Markdown references/fences, preserved approval/history sections and unchanged inline migration SQL.')
PY
git diff --check
```

Results: passed relative-reference existence, code-fence balance, preserved
approval/history markers and inline migration/source equality; diff whitespace
check passed. Application builds/CTest were not rerun for this documentation-only
step: no source, build configuration or test behavior changed. Earlier seven-pass,
one-skipped results remain the prior Step B evidence, not new validation claims.

Remaining work/approval is unchanged: obtain Stage 1 isolated live integration
approval, execute and record those checks, then request Stage 2 separately. Windows
Phase 1.1, live persistence and manual/backup checks remain unverified. Phase 1.1
is not complete. This instruction update requests no new source scope or database
authorization beyond the existing clearly labeled Approval Requested section.
