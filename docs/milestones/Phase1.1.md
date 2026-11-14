<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Phase 1.1 — Persistent Task Foundation

Status: owner reports Stage 1 integration passed and Stage 2A migration apply/verify
succeeded. Owner reports step G and the persistent GUI acceptance checklist passed;
final direct grants match Stage 2A. Stage 2B import, restored schema verification
and all-five-table comparison passed according to the owner. Phase 1.1 is not finally accepted.

Latest checkpoint: see **Owner-reported Stage 2B data comparison success** below.
Populated backup, approved import, matching restored dataset/schema and identical
full-data exports are recorded. Exact platform/server evidence and independent
Windows Phase 1.1 validation remain outstanding. No Stage 2 SQL has been run
by the agent. Previous approval-pending observations are historical, not a claim
that the owner's reported live operations did not occur.

Windows validation tooling: the source-only scripted sequence is now in
[WindowsPhase1.1.md](../WindowsPhase1.1.md). See **Scripted Windows validation
preparation — 2026-11-14** below for files, mock coverage, build/CTest evidence and
the separate Windows approval checkpoints. Owner now reports Windows stages 01–04
passed, including live integration testing; development/GUI/restore stages remain pending.
Owner reports Windows stage 01 passed after the tooling corrections: CMake 4.4.5,
GCC 16.2.0, Qt 6.12.0; 9 tests passed, 1 live test skipped, QMYSQL loadable.
Windows stages 05–07 were separately approved. Stage 05 now reports successful
baseline/provisioning/grant checks with a diagnostic-handler bug corrected below;
stage 06 migration/verification now passed with Windows dataset
457bb52b-fea4-4347-9c47-675c6b035c90. Stage 07 final grant checks now passed per
owner output, with repeated old diagnostic-handler errors; update the Windows
runner before stage 08 GUI/restart validation, which remains pending.

Source/documentation approval remains provisional. The owner's manual test result
is not authorization for Codex to provision accounts, migrate development or perform
backup restoration. Those operations retain their separate approval checkpoints.
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
| Schema and migrations | Owner reports migration 001 apply and verification succeeded in Stage 2A; no agent live execution. |
| Tests/commands/results | Executed verification and Final pre-approval verification below; later checks are appended. |
| Issues/limitations/deferred work | Owner reports Stage 2A GUI and Stage 2B restore/data checks passed; independent Windows Phase 1.1 validation and exact platform/server evidence remain outstanding. |
| Approvals and decisions | Source provisionally approved; owner approved the exact Stage 2B import and reports success. No blanket agent live-operation or cleanup authority. |
| Remaining work/completion | Record exact platform/server and any outstanding preflight evidence; independently validate Windows and obtain final owner acceptance. Preserve restored test data until separate cleanup approval. |

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

Historical initial proposal: the later Stage 1 safety review supersedes this
section's DELETE grant and startup-reset behavior. Use its revised minimal SQL
and fresh-schema procedure; the original proposal is preserved below as history.

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

## Database ER diagram — documentation update, 2026-11-12

Status: requested diagram artifacts and repository convention delivered. Phase 1.1
live database validation remains pending; this work does not approve or execute it.
Authority is migration 001 SQL, not the preliminary future logical model or an
inspected live schema. The migration and application behavior are unchanged.

### Files and implementation

- Created [DatabaseSchema.md](../diagrams/DatabaseSchema.md): Mermaid ER diagram
  with every table/column/type/nullability/key, FK and unique-constraint inventory,
  exact CHECK expressions, declared indexes, storage details and enforcement limits.
- Created [DatabaseSchema.svg](../diagrams/DatabaseSchema.svg): standalone landscape
  vector diagram, 2264.40 × 938.40 viewBox, legible table cards and labeled connectors.
  Solid blue FK links and dashed amber logical references have explicit text labels
  as well as distinct styles; the two logical self-links share one SVG connector
  for clarity. Mermaid shows them separately.
- Created [GenerateDatabaseSchema.py](../diagrams/GenerateDatabaseSchema.py):
  documentation-only Python standard-library helper. Both representations derive
  from the same parsed migration metadata. `--check` verifies generated artifacts
  byte-for-byte and checks SVG structure/content against the SQL-derived inventory.
  The reader deliberately supports migration 001 only; additional migration files
  cause refusal until cumulative-schema handling is reviewed/extended.
- Updated [DataModel.md](../DataModel.md) to link the actual SQL schema reference
  and SVG, distinguishing them from future tables in the preliminary model.
- Updated [AGENTS.md](../../AGENTS.md) to require Mermaid/SVG updates whenever a
  migration changes the schema, using cumulative migration SQL, explicit constraint
  distinctions and recorded validation. Updated this milestone with the report.

Inventory verified: **5 tables, 38 columns, 5 primary keys, 3 additional unique
constraints, 4 SQL foreign keys, 12 CHECKs and 3 declared non-unique indexes**.
Six fields are nullable. Task.datasetId/id is one composite unique key; the FK
labels preserve composite column mappings. SchemaMigration has no relationships.
Parent-to-child cardinalities come from SQL, including CommandRecord → LocalChange
zero-or-one due to UNIQUE(operationId), rather than an assumed mandatory audit pair.

The two nullable compensation/reapplication IDs are logical references, not FKs.
LocalChange.id is not an FK either; operationId is its enforced command reference.
Separate changeTask/changeCommand constraints do not enforce that the command and
change name the same task/dataset. These are documented enforcement boundaries,
not new schema-change proposals. No future domain entities were added to the diagram.

### Validation and renderer findings

Executed commands:

```bash
python3 --version
dot -V
rsvg-convert --version
python3 docs/diagrams/GenerateDatabaseSchema.py
python3 docs/diagrams/GenerateDatabaseSchema.py --check
rsvg-convert -w 2000 docs/diagrams/DatabaseSchema.svg -o /tmp/taskcompass-schema-preview.png
git diff --check
```

Results: Python 3.9.25, existing Graphviz 2.44.0 and librsvg 2.50.7 were available.
Generation and regeneration comparison passed. SVG XML parsing, table/column/key/
nullability content, relationship counts, landscape aspect and absence of external
links, DTD, images or scripts passed the helper's structural checks. An independent
Python standard-library cross-check also matched SQL columns/types/keys/nullability
to Mermaid and SVG and checked all CHECK expressions against the Markdown inventory.
Markdown local references/fences and diff whitespace checks passed.

Rendered the SVG to the temporary PNG above and visually inspected it. The initial
layout had logical self-loops crossing table text; connectors were moved outside
the card and their two logical labels grouped. The final preview has readable
labels and clear table contents without those overlaps. The preview PNG is outside
the repository; the repository artifact is the scalable SVG.

Mermaid CLI was not available. No software was installed: Graphviz generated the
SVG directly from the same SQL-derived metadata, **not by rendering Mermaid**.
Mermaid source follows the linked official ER syntax, including key markers and
nullability comments; local Mermaid rendering was not performed. Firefox interactive
opening was not performed; standard standalone SVG parsing and librsvg rendering
passed. Renderer-version changes can alter layout; regenerate/review rather than
assuming cross-version byte identity. No application build/CTest rerun was needed
for this documentation-only change; prior source-validation evidence is unchanged.

### Remaining work and approval status

No schema, migration file, application source, accounts or database contents changed.
No live migration, package installation, commit or push was performed. No conflict
with the approved architecture or new repository policies was found. Diagram
deliverables are complete, with Mermaid rendering and interactive Firefox viewing
explicitly unverified. Existing Stage 1 live-integration Approval Requested remains
pending; no new authorization is needed to create these documentation artifacts.

## Mermaid CLI installation verified — 2026-11-12

Owner reported installing Mermaid CLI. Verified `/home/justin/.nvm/versions/node/v26.13.0/bin/mmdc`
is available and reports **12.2.0**. The existing Mermaid ER source parsed and
rendered successfully. This supersedes the earlier renderer-unavailable observation
and Graphviz fallback; those earlier results remain above as history.

### Changes and decisions

- Updated `docs/diagrams/GenerateDatabaseSchema.py` to render the actual generated
  Mermaid source through the installed CLI instead of maintaining a separate DOT
  layout. It still reads migration 001 as authority; no database is contacted.
- Added `docs/diagrams/MermaidConfig.json` with a neutral theme, landscape direction
  inherited from the ER source, explicit fonts/spacing, HTML labels and deterministic
  IDs. CLI/browser dependencies are documentation tooling, not application dependencies.
- Regenerated `DatabaseSchema.svg` as native Mermaid SVG, retaining XHTML labels.
  Added an 80-pixel outer viewBox margin because CLI 12.2.0's initial bounds clipped
  the upper logical-reference label. No table/connector layout is manually redrawn.
  Four solid actual FKs and both dashed logical references are rendered separately.
- Updated `DatabaseSchema.md` regeneration/validation notes and this milestone.
  Schema content, relationships and constraints are unchanged. Earlier AGENTS/data
  model edits remain in the working tree but were not changed in this follow-up.
- Replaced Graphviz-specific byte regeneration checking with SQL/Mermaid/config
  provenance hashes plus structural/content validation of the native Mermaid SVG.
  `--check` runs without launching a browser or writing files. It checks all 38
  column types/keys/nullability/comments, five entities, four solid FK connectors,
  two dashed logical connectors, landscape bounds and no external resources/scripts.
  It does not claim byte-identical output across renderer upgrades.

### Commands, results and failures

```bash
command -v mmdc
mmdc --version
mmdc --help
```

Passed: CLI 12.2.0 installed. Extracted the Mermaid block from DatabaseSchema.md
to `/tmp/taskcompass-schema.mmd`, then tested:

```bash
mmdc -i /tmp/taskcompass-schema.mmd -o /tmp/taskcompass-mermaid-schema.svg -t neutral -b white
```

The restricted sandbox initially blocked its browser process:
`sandbox_host_linux.cc:41 ... shutdown: Operation not permitted (1)`.
The same command succeeded with an approved execution outside the restricted
sandbox. This was a browser sandbox boundary, not a missing installation or ER
syntax error. No packages were installed and no Chromium `--no-sandbox` flag was used.

Preview investigation: librsvg could not display Mermaid's XHTML labels. Trial
`htmlLabels=false` exports wrapped/clipped row comments, so that configuration was
rejected. Browser-rendered PNGs with HTML labels displayed the table contents
correctly; the final helper adds outer margin for the top self-link label. These
were documentation rendering issues, not database or application defects.

Final generation/check and browser preview commands:

```bash
python3 docs/diagrams/GenerateDatabaseSchema.py
python3 docs/diagrams/GenerateDatabaseSchema.py --check
mmdc -i /tmp/taskcompass-schema.mmd -o /tmp/taskcompass-mermaid-final.png \
   -c docs/diagrams/MermaidConfig.json -I TaskCompassSchema -t neutral -b white --size 2400
mkdir -p /tmp/taskcompass-firefox-diagram-profile
firefox --headless --no-remote --profile /tmp/taskcompass-firefox-diagram-profile \
   --screenshot /tmp/taskcompass-mermaid-firefox.png --window-size 3600,1200 \
   file:///home/justin/repo/TaskCompass/docs/diagrams/DatabaseSchema.svg
git diff --check
```

Generation and browser preview were executed with the required tool-level sandbox
approval; `--check` works within the restricted sandbox. All final commands passed.
Firefox exited successfully using a separate temporary profile; the screenshot of
the **final standalone SVG** was visually inspected and shows all text and both
logical-reference labels without clipping. Temporary screenshots/profiles remain
outside the repository. Interactive Firefox use was not tested.

Markdown relative references/fences and diff whitespace checks passed. No application
build/CTest rerun was needed: only documentation artifacts, renderer configuration
and the documentation helper changed. No schema SQL, application behavior, live
database, accounts or privileges changed; no commit or push performed.

### Status and remaining work

Mermaid rendering and headless Firefox display are now verified. Use the same SVG
link already in DataModel.md. Interactive viewing and future renderer upgrades can
be reviewed normally; neither is a new database approval. Phase 1.1 live Stage 1
integration approval remains pending and Phase 1.1 remains incomplete. No change
to previous source authorization, database conditions or milestone scope.

## Stage 1 pre-approval safety review — 2026-11-12

**Verdict: ready for owner review/approval of Stage 1, with the revised safeguards
and SQL below. No live SQL, connection, account provisioning or grant changes were
performed.** This is source-level/database-free verification, not successful live
integration validation. Stage 2 remains a separate approval.

Reviewed `tests/MariaDbIntegrationTests.cpp`, `MigrationRunner.h/.cpp`,
`DatabaseConfig.h/.cpp`, `DatabaseConnection.h/.cpp`, `MigrateMain.cpp`, migration
001, CMake test registration, safety tests and the earlier proposed grants/recovery
procedures. The migration remains unchanged; its embedded/source equality and
schema diagram checks still pass. No production schema change is proposed here.

### Findings and corrections

| Review requirement | Finding and resulting safeguard | Evidence / limitation |
| --- | --- | --- |
| Integration target only | `fromEnvironment(true)` reads only TASKCOMPASS_TEST_DB_*; exact taskcompass_integration/taskcompass_test and loopback host are required. Runtime variables never supply missing test configuration. | Pure configuration and subprocess rejection tests passed; no connection attempted. |
| Identity/account protection | Previously configuration was validated only by the environment reader; direct construction could bypass it. The connection now validates configuration before driver creation/open, then checks DATABASE()/CURRENT_USER() before session initialization or table access. | Pure tests reject development/unrelated names and wrong accounts/hosts; actual server identity execution remains untested. |
| Explicit opt-in/unexpected contents | Both exact --run --allow-reset arguments are required before reading configuration. Previously startup reset accepted any table with a recognized name, regardless of its origin. Startup now requires a completely empty schema; recognized pre-existing tables are protected too. | Pure policy tests and executable subprocess tests passed. Default integration CTest still skips without reading configuration. |
| Limited grants | The previous proposed seven-privilege grant was scoped correctly, but DELETE was unused. Revised proposal removes it: SELECT, INSERT, UPDATE, CREATE, DROP, ALTER only on the exact escaped integration schema. | Static SQL review only. Existing/effective server grants have not been queried or verified. |
| Failure/partial initialization | Migration writes now independently validate driver/configured and authenticated identities before any lock or DDL; runtime/admin credentials are refused. Existing partial/newer/checksum/fingerprint failures still stop without automatic resume or deletion. | Closed-handle rejection and embedded migration tests passed; live failure/rollback cases remain pending. |
| No development reset | Every test DROP is explicitly qualified with taskcompass_integration. Reset checks authenticated identity again, requires current-run fresh-schema ownership and rejects unknown names, views/non-InnoDB tables and visible stored code. | Source audit and pure reset-policy tests passed; no destructive SQL executed. |

Additional corrections/details:

- Added `tests/IntegrationTestSafety.h`, a database-free policy shared by the live
  test and safety tests. A fresh policy instance cannot reset anything. `begin`
  accepts only an empty catalog; it cannot be called again to adopt existing data.
  Later known test tables/partial initialization can be reset only within that
  same process. Case handling follows server lower_case_table_names; a case-sensitive
  Linux database rejects case variants instead of accidentally leaving other tables.
- Catalog inspection is explicitly filtered to taskcompass_integration and includes
  TABLES (name/type/engine), ROUTINES, TRIGGERS and EVENTS. Query failure stops the
  run. Reset validates the catalog before DROP and verifies emptiness afterward.
- A suite-lifetime advisory lock uses `TaskCompassMigration:taskcompass_integration`,
  the same key as MigrationRunner. Competing cooperating suites/migration applies
  fail before changes. Nested acquisition on the same connection is recursive;
  inner migration release leaves the suite lease held. A second connection tests
  exclusion in the pending live suite. See [MariaDB GET_LOCK](https://mariadb.com/docs/server/reference/sql-functions/secondary-functions/miscellaneous-functions/get_lock).
- `DatabaseConfig::validateIdentity` rejects wrong selected schemas and mismatched
  authenticated accounts, including localhost/root/other identities. No credentials
  or actual identity values are printed in failure messages.
- Connections explicitly enable foreign_key_checks in addition to CHECK enforcement;
  migration writes and test catalog/reset checks require both enabled. Tests never
  disable FK checks to force a DROP. A dependency error stops instead of cascading
  into another schema.
- `MigrationRunner::verify` also checks identity before inspecting tables. Raw
  QSqlDatabase callers cannot accidentally migrate an arbitrary selected database;
  supported development runtime reads remain permitted, while write access requires
  the dedicated migration/test account. The SELECT-only diagnostic is unchanged.

### Partial-failure and recovery assessment

Migration 001 contains only the five reviewed CREATE TABLE statements, InnoDB,
utf8mb4, same-schema FKs/indexes/CHECKs and the prepared metadata inserts already
recorded above. There is no DROP, ALTER, GRANT, account operation, USE or unrelated
schema reference in migration SQL. No migration/source checksum changes were made.

DDL may already be committed when a later statement fails. The tool never claims
DDL rollback: it records success only after creating/verifying the schema and
committing Dataset/SchemaMigration metadata together. A partial existing table set,
missing metadata, unexpected table, checksum/fingerprint mismatch or unsupported
version is rejected, not repaired. Metadata commit uncertainty requires inspection.
The existing backup/restore and separately approved cleanup procedure remains valid.
No backup/restore, partial-failure SQL or repair was executed during this review.

**Important recovery change:** after a failed integration run, a new invocation
will refuse any leftover tables, even known ones. It will not use --allow-reset as
permission to erase previous-run evidence. Preserve the failure and obtain a separate,
concrete recovery approval for leftover integration objects before another run.
Within one approved run, deliberately injected partial DDL/fault states may be reset
because the suite previously established its own fresh schema and still holds its
lease. This distinction does not authorize cleaning up development data.

### Files changed in this review

- `src/infrastructure/storage/DatabaseConfig.h/.cpp`: reusable preconnection and
  authenticated-identity validation.
- `src/infrastructure/storage/DatabaseConnection.cpp`: enforce those guards and
  foreign-key checking before domain/table work.
- `src/infrastructure/storage/MigrationRunner.h/.cpp`: independent identity/role/
  constraint guards on verify/apply boundaries.
- New `tests/IntegrationTestSafety.h`; updated `tests/MariaDbIntegrationTests.cpp`:
  exact opt-in, fresh-start policy, catalog checks, suite lease, qualified DROP,
  retained failure evidence and post-reset emptiness check.
- `tests/StorageSafetyTests.cpp`: pure guards plus blocked subprocess cases;
  `CMakeLists.txt`: pass the integration executable to these tests and ensure it
  is built first. Default live test registration remains OFF.
- `docs/Building.md`: fresh-start, no automatic recovery and concurrency/metadata
  caveats. This milestone records findings and the superseding approval material.

No domain/application/GUI feature code, migration SQL, diagram schema, diagnostic
account or unrelated project was changed. The shared storage boundary now rejects
unsafe configurations earlier; ordinary approved runtime configuration is preserved.

### Exact executed commands and actual results

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DTASKCOMPASS_ENABLE_DB_TESTS=OFF
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/StorageSafetyTests migrations/mariadb/001_PersistentTaskFoundation.sql ./build/MariaDbIntegrationTests
python3 docs/diagrams/GenerateDatabaseSchema.py --check
git diff --check
```

Configured with live registration explicitly OFF before CTest, avoiding a stale
cache enabling destructive tests. Linux build succeeded. CTest: **seven passed,
one skipped (MariaDbIntegrationTests), zero failures**. StorageSafetyTests covers:

- Valid test configuration versus development/unrelated database, root/diagnostic/
  other accounts, non-loopback host, invalid port and missing explicit isolation.
- Direct-constructor rejection before opening, authenticated identity mismatch and
  unopened migration handles failing before SQL.
- Reset-before-begin rejection; protected recognized/unknown pre-existing tables;
  routines/triggers/events; wrong table case, view type, non-InnoDB engine; expected
  in-run partial table acceptance and Windows case-insensitive behavior.
- Subprocesses with no flags, one flag, reversed flags and extra flags exit 77;
  both flags with runtime-only environment or invalid test schema/user/host exit 1
  at configuration validation. Only fake test credentials are used, never printed;
  no valid connection case is launched by these process tests.

Actual MariaDB authentication/current-user handling, object visibility, lock
recursion/exclusion, grant enforcement, migration DDL, rollback/fault injection and
Windows execution remain **not run live**. Database-free tests validate decisions
and guard paths; source review does not prove a live server is correctly configured.

### Limitations and required operator preflight

Named locks are cooperative, not protection against an administrator ignoring them.
No other writer/admin process may modify the disposable schema during the approved
suite. Known-table shape is not frozen during in-run reset because the suite
intentionally changes it to test migration drift; fresh start plus exclusive use
establishes ownership. Stop on unexpected interference rather than trusting names.

Restricted metadata may not expose every stored object/effective grant. The test's
catalog rejection is defense in depth, not a server-wide security audit. The approved
administrator must confirm empty objects, account scope and inherited role/PUBLIC
policy before allowing the first test. Do not grant TRIGGER/EVENT/ROUTINE/global
rights merely to improve visibility. See MariaDB's [trigger metadata](https://mariadb.com/docs/server/reference/system-tables/information-schema/information-schema-tables/information-schema-triggers-table).
Unrelated PUBLIC test/test_% grants are not automatically modified; no test query
uses those databases. Unexpected effective rights affecting development or broader
write authority require explicit review, not silent privilege repair.

## Approval Requested — Stage 1 after safety review

**Ready for approval, not approved or executed.** This supersedes the earlier Stage 1
startup-reset procedure and DELETE grant. Request only: approved administrator
preflight and fresh integration provisioning, followed by the guarded Linux live
suite below. Do not access/migrate taskcompass_dev or provision runtime/migration
accounts under this Stage 1 approval. Stage 2 remains separate.

Future read-only administrator preflight (do not execute until approved):

```sql
SELECT VERSION();
SELECT SCHEMA_NAME, DEFAULT_CHARACTER_SET_NAME, DEFAULT_COLLATION_NAME
   FROM information_schema.SCHEMATA WHERE SCHEMA_NAME='taskcompass_integration';
SELECT TABLE_NAME,TABLE_TYPE,ENGINE FROM information_schema.TABLES
   WHERE TABLE_SCHEMA='taskcompass_integration';
SELECT ROUTINE_NAME FROM information_schema.ROUTINES
   WHERE ROUTINE_SCHEMA='taskcompass_integration';
SELECT TRIGGER_NAME FROM information_schema.TRIGGERS
   WHERE TRIGGER_SCHEMA='taskcompass_integration';
SELECT EVENT_NAME FROM information_schema.EVENTS
   WHERE EVENT_SCHEMA='taskcompass_integration';
SELECT User,Host FROM mysql.user WHERE User='taskcompass_test';
```

If the database or any same-name account already exists, stop for inspection and
explicit reuse/recovery approval; do not overwrite it. For confirmed absent
resources only, the revised provisioning SQL is:

```sql
CREATE DATABASE taskcompass_integration
   CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER 'taskcompass_test'@'127.0.0.1'
   IDENTIFIED BY '<distinct-strong-test-password>';
GRANT SELECT, INSERT, UPDATE, CREATE, DROP, ALTER
   ON `taskcompass\_integration`.*
   TO 'taskcompass_test'@'127.0.0.1';
```

This is the entire requested privilege scope: exact integration schema only, no
DELETE, global grant, GRANT OPTION, TRIGGER/EVENT/ROUTINE or development rights.
Each privilege is exercised by the test: reading; inserting domain/audit/metadata;
updating tasks and fault fixtures; initial CREATE; known-table DROP; drift-test
ALTER. DROP also grants powerful authority within the disposable schema, so the
account is test-only. The test contains no DROP DATABASE or account/grant SQL.
Keep the existing diagnostic/runtime/migration users unchanged. Supply the real
password through a reviewed private stdin mechanism, never source, shell history,
command arguments, logs or Git; the placeholder is not a usable literal password.

After authorized provisioning, inspect direct USER_PRIVILEGES/SCHEMA_PRIVILEGES/
TABLE_PRIVILEGES/COLUMN_PRIVILEGES and role mappings for this exact test account;
expect no global authority and only the six schema rights above, without grant
options or extra roles. On MariaDB 10.11+ review PUBLIC grants (and any inherited
roles) too; do not issue a PUBLIC query on 10.5 where that role is unavailable.
Do not print account authentication hashes. Stop on unreadable/unexpected policy;
no automatic privilege repair is authorized. Live grant correctness is an acceptance
check still to perform, not a result of this review.

Exact future privilege-metadata queries (administrator, after approval only):

```sql
SET @testGrantee = CONCAT(CHAR(39),'taskcompass_test',CHAR(39),'@',CHAR(39),'127.0.0.1',CHAR(39));
SELECT PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.USER_PRIVILEGES
   WHERE GRANTEE=@testGrantee;
SELECT TABLE_SCHEMA,PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.SCHEMA_PRIVILEGES
   WHERE GRANTEE=@testGrantee;
SELECT TABLE_SCHEMA,TABLE_NAME,PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.TABLE_PRIVILEGES
   WHERE GRANTEE=@testGrantee;
SELECT TABLE_SCHEMA,TABLE_NAME,COLUMN_NAME,PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.COLUMN_PRIVILEGES
   WHERE GRANTEE=@testGrantee;
SELECT Host,User,Role FROM mysql.roles_mapping WHERE User='taskcompass_test';
```

Only when the server version supports PUBLIC (10.11+, including 13.0.2):

```sql
SHOW GRANTS FOR PUBLIC;
```

The direct schema name should be the exact escaped grant pattern, with six rights
and IS_GRANTABLE=NO; no account-global rights except possible USAGE, and no table/
column/explicit role grants are expected. If PUBLIC grants roles, stop for a complete
inherited-policy review before the live suite; do not assume its first-level listing
is sufficient. These read-only privilege queries deliberately avoid raw account
SHOW GRANTS output containing authentication material.

Once approved and confirmed fresh/empty, exact test command:

```bash
export TASKCOMPASS_TEST_DB_HOST=127.0.0.1 TASKCOMPASS_TEST_DB_PORT=3306
export TASKCOMPASS_TEST_DB_NAME=taskcompass_integration
export TASKCOMPASS_TEST_DB_USER=taskcompass_test TASKCOMPASS_TEST_DB_ISOLATED=1
read -r -s -p 'Integration database password: ' TASKCOMPASS_TEST_DB_PASSWORD
export TASKCOMPASS_TEST_DB_PASSWORD
./build/MariaDbIntegrationTests --run --allow-reset
unset TASKCOMPASS_TEST_DB_PASSWORD
```

Do not run yet. `dev-environment.sh` selects the diagnostic configuration and is not
a substitute for these explicit TEST variables. Windows later uses the same TEST
variables and `./build-ucrt64/MariaDbIntegrationTests.exe --run --allow-reset`.

Approved in-run resets would use only these fully qualified statements after all
identity/catalog/ownership checks and while holding the migration lease:

```sql
DROP TABLE IF EXISTS `taskcompass_integration`.`LocalChange`;
DROP TABLE IF EXISTS `taskcompass_integration`.`CommandRecord`;
DROP TABLE IF EXISTS `taskcompass_integration`.`Task`;
DROP TABLE IF EXISTS `taskcompass_integration`.`Dataset`;
DROP TABLE IF EXISTS `taskcompass_integration`.`SchemaMigration`;
```

A failed first run leaves evidence, not an automatically reusable empty schema.
Request separate approved cleanup if needed; never target development for test
recovery. On success the test verifies the integration schema is empty. Record
actual Stage 1 results here and stop before Stage 2. No commit, merge, tag or push.

Final review checks: the direct StorageSafetyTests command above passed, as did
the browser-free diagram/source consistency check. Updated Markdown references,
fences, the latest six-privilege isolated GRANT text and inline/source migration
equality were checked with Python's standard library and passed. `git diff --check`
passed; `git diff -- migrations/mariadb/001_PersistentTaskFoundation.sql` is empty.
No build/test failures were observed. Review is complete; await explicit Stage 1
approval before any live preflight SQL, provisioning or integration execution.

## Provisional source approval and validation sequence confirmed — 2026-11-12

Owner provisionally approves Phase 1.1 source implementation and documentation as
ready for validation. This is **not final milestone acceptance**, nor permission
for live SQL, account provisioning or destructive integration tests. The owner
reaffirmed the existing checkpoints and all remaining acceptance criteria.

Re-inspected the current integration opt-in/reset policy, connection configuration
and authenticated-identity checks, migration guards and test registration against
the completed Stage 1 safety review. No additional corrections were identified.
The earlier findings and fixes remain applicable: fresh empty schema at startup,
current-process ownership before reset, exact database/account validation, qualified
integration-only DROP, unexpected-object rejection, cooperative migration lease,
constraint enforcement and six scoped test privileges (no DELETE/global authority).
Actual server identity, metadata visibility and effective grants still require
approved live preflight; source review does not establish those server facts.

The migration file's current SHA-256 remains
`bb901d7580398893b5c04ee263f813b9d05873eb4bae08e0daf377faf73b0ea0`,
matching the reviewed DDL and diagram provenance. Current source review is complete
and Stage 1 remains ready for approval. No live SQL, connection, provisioning or
integration reset has occurred in this confirmation step.

### Required sequence and completion gates

| Gate | Current status / required next action |
| --- | --- |
| Source/documentation review | Provisionally approved for validation; no final acceptance. |
| Stage 1 safety review | Complete with previously documented corrections; no new corrections in this confirmation. |
| Stage 1 live preflight/provisioning/integration | Pending explicit approval of the exact operations in the preceding Stage 1 Approval Requested section. Stop if target/account already exists or preflight differs from expectations. |
| Stage 1 results | Run only after approval; record actual Linux/MariaDB results, errors and any recovery request here. A failure does not authorize automatic cleanup/retry. |
| Stage 2 development migration | Only after Stage 1 passes, finalize the development-specific SQL, privileges, backup/recovery procedure and commands for separate approval. Earlier Stage 2 text is a proposal, not execution authority. |
| Linux and Windows foundation validation | Both required. Accepted Phase 0.1 prototype results do not validate the new persistent foundation or safety changes on Windows. |
| Persistent GUI and undo/redo | Validate real capture/edit/complete/reopen, filtering/selection, all supported inverses/redos and failure handling against persisted data. Current fake-repository GUI tests are not sufficient. |
| Restart persistence | Verify actual application close/restart preserves stable IDs, fields, timestamps and history; session undo remains intentionally nonpersistent. |
| Backup/restore | Execute the reviewed, separately authorized backup/restore validation and retain evidence before relying on recovery. Not yet performed. |
| Final acceptance | Phase 1.1 remains incomplete until required checks pass and results are reviewed. No Phase 1.2 work authorized. |

Only `docs/milestones/Phase1.1.md` changed in this confirmation. No source/schema
change or fresh application build/CTest result is claimed; those tests were not
rerun because the source is unchanged. The preceding seven-pass/one-skipped results
remain historical validation evidence. This step checked the migration SHA-256,
Markdown references/fences and diff whitespace; commands/results follow:

```bash
sha256sum migrations/mariadb/001_PersistentTaskFoundation.sql
python3 - <<'PY'
from pathlib import Path
import re
path = Path('docs/milestones/Phase1.1.md')
text = path.read_text()
assert sum(line.startswith('```') for line in text.splitlines()) % 2 == 0
for target in re.findall(r'\]\(([^)]+)\)', text):
    if '://' not in target and not target.startswith('#'):
        assert (path.parent / target.split('#')[0]).exists(), target
assert '## Approval Requested — Stage 1 after safety review' in text
assert Path('migrations/mariadb/001_PersistentTaskFoundation.sql').read_text() in text
print('PASS: milestone references/fences, retained approval request and migration SQL.')
PY
git diff --check
```

Results: migration hash matched; document consistency and whitespace checks passed.
No new validation failures or architecture conflicts were found. No live database
state has been queried or inferred. No commit, merge, tag or push performed.

### Approval Requested — execute the reviewed Stage 1 operations

Await explicit owner authorization for the **exact Stage 1 SQL and command blocks
in “Approval Requested — Stage 1 after safety review” above**: integration-only
administrator preflight; conditional fresh taskcompass_integration database and
`taskcompass_test@127.0.0.1` provisioning; SELECT/INSERT/UPDATE/CREATE/DROP/ALTER on
that exact escaped schema; effective-privilege inspection; and the guarded Linux
`./build/MariaDbIntegrationTests --run --allow-reset` invocation, including its
reviewed migration/fault-injection DDL/DML and in-run qualified test-table resets.

This request excludes development migration, existing-resource overwrite, failure
cleanup, new/global privileges, unrelated databases and Stage 2. Provisional source
approval does not authorize any of these live operations. Stop here until the
owner explicitly approves the reviewed Stage 1 scope.

## Operator testing walkthrough — 2026-11-12

Owner requested instructions for testing, including database setup. Added
[TestingPhase1.1.md](../TestingPhase1.1.md) and linked it from Building.md. The
walkthrough includes Linux/UCRT64 build commands, explicit live-test registration
OFF, service checks, administrator access, fresh-resource preflight, the reviewed
integration-only CREATE/GRANT statements, optional TCP authentication, hidden test
password entry, the exact opt-in executable command, exit-code capture and failure
handling. It distinguishes test/administrator/diagnostic passwords and the later
Stage 2 persistent GUI workflow. No setup or live test was executed.

The guide uses the existing six-privilege proposal unchanged. It does not tell the
operator to reuse the diagnostic setup script for integration, alter its account,
manually migrate the test schema before the suite, or erase failure leftovers.
Linux administrator client examples disable client history; Windows/native client
examples use NUL. Password examples remain placeholders. The owner controls manual
execution; requesting a guide is not authorization for Codex to execute live SQL.

Files changed in this step: new `docs/TestingPhase1.1.md`, updated
`docs/Building.md` and this milestone. The preceding provisional-approval record
was already uncommitted and is preserved. No source, schema or application behavior
changed; no accounts/databases were accessed or provisioned, no dependencies
installed and no Git commit/push performed.

Validation: compared commands/flags/TEST variables and expected exit behavior to
DatabaseConfig, integration main and CMake; checked the fresh setup SQL against
the latest Stage 1 approval section. Python standard-library relative-link/fence
checks and `git diff --check` passed. Shell/SQL snippets were reviewed as text,
not executed. No build/CTest rerun was needed for documentation-only instructions;
the earlier seven-pass/one-skipped evidence is unchanged. No new validation result
for a live database or Windows is claimed.

Remaining gates are unchanged: Stage 1 exact operations require explicit approval;
after integration passes, finalize Stage 2 for separate approval. Persistent GUI,
undo/redo, restart persistence, Linux/Windows and backup/restore checks remain
required before Phase 1.1 acceptance. Phase 1.2 remains unauthorized.

## Integration setup/environment/test helpers — 2026-11-12

Owner authorized creation of helper files under `temp/testdb`. This authorizes
source/preparation only; it is **not** approval to execute live provisioning or
integration tests. Created the helpers, reviewed their paths/secret handling and
validated them with mocked executables. No live MariaDB client, SQL, account change,
migration or destructive integration invocation was used.

### Files and behavior

Created in the requested repository-local temporary directory:

- `temp/testdb/setup.template.sql`: reviewed CREATE DATABASE/CREATE USER/six-right
  GRANT template with `{{TEST_PASSWORD_SQL_LITERAL}}`, never a saved actual password.
  This is intentionally not directly executable SQL; do not SOURCE it or replace
  its token on disk. The session-only NO_BACKSLASH_ESCAPES mode permits safe
  single-quote doubling in the in-memory password literal.
- `temp/testdb/setup.sh`: no-op/help by default; --print-plan displays placeholder
  SQL without contacting a client. --apply requires explicit typed confirmation,
  an authorized admin client and hidden passwords. It checks the server port and
  absence of the exact target schema/any same-name test account before rendering
  SQL in memory and piping it to the client. Uses --no-defaults, explicit socket
  or loopback TCP, utf8mb4, stdin SQL and per-child MYSQL_PWD, never password arguments
  or files. Successful stderr warnings stay out of query result parsing; errors
  redact original/SQL-escaped passwords. Existing/ambiguous resources fail closed.
  No IF NOT EXISTS, password replacement, migrations, tests or automatic recovery.
- `temp/testdb/env.sh`: source-only optional hidden password prompt and non-secret
  TASKCOMPASS_TEST_DB_* exports; no files/profiles written. Refuses shell tracing,
  clears cancelled/empty password input, and provides clearTaskCompassTestPassword.
- `temp/testdb/test.sh`: requires --run --allow-reset, rejects conflicting TEST
  settings, selects the Linux/UCRT64 build or an explicit --build-dir, prompts only
  if no test password is loaded, and calls the existing guarded executable. It
  preserves 0/1/77 statuses and clears its process password on exit. A child cannot
  clear the parent's environment: callers who source a password must clear it in
  that shell afterward.
- `temp/testdb/README.md`: full commands, approval boundaries, fresh-schema/partial
  failure behavior, native Windows paths, password handling and test limitations.
- `temp/testdb/.gitignore`: allowlists the six reviewed helper files and ignores
  other local files/output. This is not permission to store passwords in ignored
  files; no helper writes secret SQL/configuration or test logs.

Also created `tests/IntegrationDatabaseScripts.test.sh` and registered its mock
suite in CMake. Updated TestingPhase1.1.md to link the helpers and reflect the new
non-live test count; updated this milestone. Earlier uncommitted Building.md and
walkthrough changes remain preserved, not new behavior changes from this step.
No production C++ or migration schema changes; diagrams need no schema update.

### Exact proposed helper SQL and execution boundary

Preflight issued only by an explicitly approved --apply invocation:

```sql
SELECT @@port,
   (SELECT COUNT(*) FROM information_schema.SCHEMATA WHERE SCHEMA_NAME='taskcompass_integration'),
   (SELECT COUNT(*) FROM mysql.user WHERE User='taskcompass_test');
```

Only an exact `3306, 0, 0` result permits provisioning. The SQL template contains:

```sql
SET SESSION sql_mode='NO_BACKSLASH_ESCAPES';
CREATE DATABASE taskcompass_integration
   CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER 'taskcompass_test'@'127.0.0.1'
   IDENTIFIED BY {{TEST_PASSWORD_SQL_LITERAL}};
GRANT SELECT, INSERT, UPDATE, CREATE, DROP, ALTER
   ON `taskcompass\_integration`.*
   TO 'taskcompass_test'@'127.0.0.1';
```

The token becomes a quoted, escaped literal only in memory/SQL stdin. The script
writes no rendered copy. Separate statements can partially commit: a concurrent
create or later error stops; inspect the result rather than retry or grant to an
unknown account. This is the same isolated schema/account/privilege proposal; no
additional database authority is requested. Full administrator role/PUBLIC checks
from the safety review are still required before live testing. These helpers do
not claim to automate or validate all inherited privileges.

### Validation actually executed

```bash
bash -n temp/testdb/setup.sh temp/testdb/env.sh temp/testdb/test.sh tests/IntegrationDatabaseScripts.test.sh
bash tests/IntegrationDatabaseScripts.test.sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DTASKCOMPASS_ENABLE_DB_TESTS=OFF
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
git check-ignore temp/testdb/password.env temp/testdb/test-output.log
git diff --check
```

Results: shell syntax and all five mock-test groups passed. Tests cover no-action
and plan defaults; required opt-in/confirmation; fresh setup, CRLF/stdout/stderr;
refusal of existing resources/failed inspections/cancelled confirmation/short or
mismatched passwords; correct quote/backslash/ampersand SQL escaping; password
redaction and no password arguments; conflicting-target refusal, paths with spaces,
exit-status propagation and source-only/cancelled-input credential cleanup.
Only fake credentials and fake client/test executables were used. Temporary test
state is outside the repository and removed by the mock suite.

Linux CMake configure/build passed. CTest now has **eight passed, one skipped
(MariaDbIntegrationTests), zero failures**, including the new IntegrationDatabaseScripts
suite. Subsequent focused mock reruns passed after utf8mb4/client and cancelled-input
handling refinements. Git-ignore checks confirm unknown password/output paths are
ignored while reviewed helper source paths are not. Markdown relative references,
code fences and whitespace checks passed. No live Linux/Windows helper setup or
real integration result is claimed; Windows execution remains pending.

### Approval Requested — live helper use remains pending

The helper files are ready for owner review. After explicit Stage 1 approval, the
proposed Linux commands would be:

```bash
sudo bash temp/testdb/setup.sh --apply --admin-socket --client /usr/bin/mariadb
# After the administrator grant/inherited-policy checks from the safety review:
bash temp/testdb/test.sh --run --allow-reset --build-dir build
```

Without execution authority, safe inspection remains:

```bash
bash temp/testdb/setup.sh --print-plan
```

The environment helper can be sourced with --no-password without SQL; --password
loads a secret only into the current shell, never a file. Stage 1 still requires
explicit approval for provisioning and the destructive isolated suite. Stage 2,
existing-resource recovery and development migration remain separate checkpoints.
No credentials were read from existing applications, no dependency installed and
no commit, merge, tag or push performed. Phase 1.1 remains incomplete.

## Full testing sequence and print-plan clarification — 2026-11-13

Owner asked whether --print-plan shows what will happen and requested all testing
steps. Verified the current helper with:

```bash
bash temp/testdb/setup.sh --print-plan
```

Result: printed only the provisioning SQL template and password token; exit 0.
The branch exits before discovering a client, checking resources or prompting for
credentials. No SQL/session statement in the printed text was executed. It is not
an assessment of the current server and does not show/run table migrations or tests.

Updated `docs/TestingPhase1.1.md` with that distinction, the whole-milestone
sequence and the exact administrator privilege-metadata checks already proposed
in this milestone. Existing setup/test commands and approval boundaries are
unchanged. The practical Linux sequence is build/non-live tests, review setup plan,
confirm Stage 1 authority, service/admin preflight, fresh setup helper, review
effective grants, then the explicit integration runner. Do not manually create
integration tables before the suite. Failed evidence is preserved, not automatically
reset. UCRT64 uses its existing documented platform paths/build directory.

Stage 1 success must be reviewed before finalizing Stage 2 development migration,
separate runtime/migration accounts and backup/recovery execution for approval.
Real GUI capture/edit/completion/reopen, all supported undo/redo, restart persistence,
Linux/Windows and backup/restore checks still precede final milestone acceptance.
Only the accepted in-memory prototype can be demonstrated without that migration;
it is not a substitute for persistent application validation.

Files changed in this clarification: TestingPhase1.1.md and this milestone. Existing
uncommitted helper/build/walkthrough changes are preserved. Markdown relative
references/fences and `git diff --check` passed. Application builds/CTest were not
rerun because source/test behavior is unchanged; earlier eight-pass/one-skipped
results remain prior evidence. No live SQL, accounts, databases, package installs,
commits, merges, tags or pushes. No new approval is inferred from the request for
instructions; live Stage 1 operations remain explicitly approval-gated for Codex.


## Owner-reported Stage 1 integration success — 2026-11-13

Owner supplied this exact output (no credentials):

```text
$ temp/testdb/test.sh --run --allow-reset --build-dir build
Integration database password:
Running disposable integration tests on taskcompass_integration@127.0.0.1:3306.
PASS: MariaDB migrations/version/checksum/fingerprint, persistence/restart, commands, constraints and atomic rollback. Test schema left empty.
Integration test exit code: 0
```

Result: **owner-executed integration run passed**, exit 0; the executable reports
its disposable schema was left empty. This covers the compiled suite's migration,
version/checksum/fingerprint, lifecycle/commands, transactional failure and new-
connection reload checks. It does not establish real GUI restart behavior or backup
restoration. The agent did not execute the live run or independently inspect the
server. Platform, exact server version and privilege-metadata results are not in
the transcript; requested platform confirmation rather than claiming both Linux
and Windows passed. The `build` directory name alone does not prove platform.

Earlier Stage 1 pending entries remain historical. Do not rerun provisioning:
this result implies a working configured test account/database, not that resources
are absent. It does not authorize Stage 2 or retrospective agent database access.
Phase 1.1 remains incomplete and source acceptance remains provisional.

## Stage 2 preparation — 2026-11-13

Reviewed the migration CLI, connection/identity guards, persistent main composition,
actual migration SQL and existing privilege proposal. Prepared the concrete Stage 2A
plan below without changing source or connecting to a server. Migration 001 remains
SHA-256 `bb901d7580398893b5c04ee263f813b9d05873eb4bae08e0daf377faf73b0ea0`.
Its exact DDL and prepared metadata transaction are already reproduced above and
in `migrations/mariadb/001_PersistentTaskFoundation.sql`; no new migration is proposed.

Checked the installed dump tool with `command -v mariadb-dump` and
`mariadb-dump --no-defaults --help`: client 10.5.29 supports the flags below. Help
only; no database connection or dump was attempted. Reviewed primary documentation
for [GRANT](https://mariadb.com/docs/server/reference/sql-statements/account-management-sql-statements/grant)
and [mariadb-dump](https://mariadb.com/docs/server/clients-and-utilities/backup-restore-and-import-clients/mariadb-dump).
CREATE can be granted before tables exist; the proposed metadata table grants
include it. Single-transaction backups require no concurrent DDL. Neither this
review nor the Stage 1 transcript proves the new minimal-account grants or dump/
restore procedures work live; those are Stage 2 acceptance checks.

### Approval Requested — Stage 2A development foundation

**Proposed, not executed.** Request approval for the exact preflight, private empty-
baseline backup, two fresh scoped accounts, migration 001, runtime grants and
migrator write-privilege removal below, followed by owner-operated persistent GUI
checks. Initial execution target is the Linux development host/loopback TCP 3306.
Confirm that Stage 1 passed on that host/version before using its result as a local
compatibility prerequisite; otherwise first obtain the missing platform run.
Windows commands are documented separately below but Windows success is not claimed.

No DROP/ALTER or existing-account password/grant overwrite is requested. Preserve
`taskcompass_dev@127.0.0.1` and all its existing SELECT-only diagnostic configuration.
Stage 2B restore rehearsal, failure cleanup, any live development restoration and
unrelated database access are excluded from Stage 2A execution authority.

#### A. Read-only administrator preflight

Use an explicitly authorized administrator for these setup/metadata operations,
never for running TaskCompass or TaskCompassMigrate. Stop other TaskCompass writers.
The database must already exist, be empty and have the expected character settings;
if not, stop for review rather than recreating it or adopting existing contents.

```sql
SELECT VERSION(), @@port, @@lower_case_table_names;
SELECT SCHEMA_NAME,DEFAULT_CHARACTER_SET_NAME,DEFAULT_COLLATION_NAME
   FROM information_schema.SCHEMATA WHERE SCHEMA_NAME='taskcompass_dev';
SELECT TABLE_NAME,TABLE_TYPE,ENGINE FROM information_schema.TABLES
   WHERE TABLE_SCHEMA='taskcompass_dev';
SELECT ROUTINE_NAME FROM information_schema.ROUTINES
   WHERE ROUTINE_SCHEMA='taskcompass_dev';
SELECT TRIGGER_NAME FROM information_schema.TRIGGERS
   WHERE TRIGGER_SCHEMA='taskcompass_dev';
SELECT EVENT_NAME FROM information_schema.EVENTS
   WHERE EVENT_SCHEMA='taskcompass_dev';
SELECT User,Host FROM mysql.user
   WHERE User IN ('taskcompass_dev','taskcompass_migrate','taskcompass_app');
```

Expected: intended MariaDB server on 3306; exactly taskcompass_dev with utf8mb4 /
utf8mb4_unicode_ci; no tables/views, routines, triggers or events; the existing
127.0.0.1 diagnostic account; **no** account of either new name on any host. If new
accounts already exist, stop for explicit inspection/reuse approval, not CREATE
USER IF NOT EXISTS or ALTER USER. Review current role/PUBLIC inheritance as in the
Stage 1 policy; do not expose authentication hashes or modify PUBLIC to proceed.

#### B. Preserve the confirmed empty baseline

No application data should exist yet. Use the existing SELECT-only diagnostic
account to take a private logical backup; do not change its privileges. Choose a
new directory outside the repository, not temp/testdb. Proposed Linux commands:

```bash
taskCompassBackupDirectory="$HOME/temp/TaskCompassBackups/Phase1.1-$(date +%Y%m%d-%H%M%S)"
(
   umask 077
   mkdir -p -- "$(dirname "$taskCompassBackupDirectory")" &&
   mkdir -- "$taskCompassBackupDirectory" &&
   mariadb-dump --no-defaults --protocol=TCP --host=127.0.0.1 --port=3306 \
      --user=taskcompass_dev --password --default-character-set=utf8mb4 \
      --single-transaction --skip-lock-tables --no-tablespaces --skip-triggers \
      --skip-add-drop-table --skip-add-locks --skip-disable-keys \
      --no-create-db --hex-blob \
      --result-file="$taskCompassBackupDirectory/before-migration.sql" taskcompass_dev
)
backupExitCode=$?
printf 'Backup exit code: %s\n' "$backupExitCode"
```

The leaf directory is created exclusively; if it exists, the && chain stops rather
than overwriting a previous dump. Enter the existing **diagnostic password** at its
hidden prompt. Proceed only on
exit 0, a nonempty dump file and inspection confirming the empty baseline. Record:

```bash
test -s "$taskCompassBackupDirectory/before-migration.sql"
sha256sum "$taskCompassBackupDirectory/before-migration.sql"
less "$taskCompassBackupDirectory/before-migration.sql"
```

No terminal output after the password prompt is normal: --result-file writes the
dump to disk. A successful test -s also prints nothing. Require both exit codes
to be zero and review the dump before proceeding; a checksum alone does not prove
a successful backup. Press q to leave less. Use a private, nonsynchronized backup
location; this owner-selected path remains outside the repository.

Record preflight charset/collation separately. Do not use --databases or --all-
databases: this proposal intentionally avoids source-database USE/CREATE statements
and any unrelated schema/account data. Stop if inspection finds otherwise. Treat
failed/partial dumps as unusable. Restrict access to the backup directory; the dump
contains user data after later saves, but never account credentials by design.
The initial empty backup is baseline evidence, **not** proof of full data restoration.
A populated restore rehearsal remains required before final acceptance and before
relying on this procedure to recover important data.

#### C. Create the migration account with narrowly scoped rights

Only after the preflight/backup gates, and with credentials supplied privately
through the reviewed history-safe/stdin procedure (placeholders are not literal
passwords):

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

This carries forward the reviewed privilege design: schema-scoped CREATE/SELECT,
INSERT only on the two metadata tables; table-level CREATE also permits the
pre-creation grants. No ALTER, DROP, DELETE, UPDATE, global privileges, GRANT OPTION
or migration use of root. Inline indexes are covered by CREATE. Read-only schema
verification needs SELECT/SHOW CREATE on all five tables. Check these grants live;
a refusal is a stop/inspect condition, not permission to broaden them.

#### D. Apply migration 001 using the normal-user CLI

Use a new terminal/subshell if earlier DB environment is loaded. The migration
user/password are different from administrator, test and diagnostic credentials:

```bash
set +x
export TASKCOMPASS_DB_HOST=127.0.0.1 TASKCOMPASS_DB_PORT=3306
export TASKCOMPASS_DB_NAME=taskcompass_dev TASKCOMPASS_DB_USER=taskcompass_migrate
export TASKCOMPASS_DB_ISOLATED=1
read -r -s -p 'Migration account password: ' TASKCOMPASS_DB_PASSWORD
printf '\n'
export TASKCOMPASS_DB_PASSWORD
migrationExitCode=0
./build/TaskCompassMigrate --apply && ./build/TaskCompassMigrate --verify || migrationExitCode=$?
unset TASKCOMPASS_DB_PASSWORD
printf 'Migration/verification exit code: %s\n' "$migrationExitCode"
```

Do not run the CLI with sudo/admin credentials, manually pipe the DDL file to the
client, or use --integration here. Its identity guard enforces the dedicated role,
versions/checksums/fingerprint and selected database. Expected: schema 1 verified,
one dataset UUID, five tables and no tasks. Record UUID/checksum/output without
credentials. Capture each exit status; stop and clear the password on any failure.

#### E. Create the runtime account after verified migration

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

Runtime can update Task/tombstones and append audit/change rows. It cannot update/
delete history, edit schema/dataset metadata, physically delete tasks or execute DDL.
Validate effective privileges before testing. Do not reuse the migration account
for the GUI or add runtime CREATE to fix schema initialization errors.

Once migration is verified, remove the migrator's initial write rights explicitly:

```sql
REVOKE CREATE ON `taskcompass\_dev`.* FROM 'taskcompass_migrate'@'127.0.0.1';
REVOKE CREATE, INSERT ON taskcompass_dev.Dataset FROM 'taskcompass_migrate'@'127.0.0.1';
REVOKE CREATE, INSERT ON taskcompass_dev.SchemaMigration FROM 'taskcompass_migrate'@'127.0.0.1';
```

Migrator retains SELECT for verification only. Future migrations need their own
reviewed grants; do not automatically restore write privileges. Disconnect/reconnect
when checking the final permission state.

#### F. Verify direct and inherited privileges

Administrator metadata queries (read-only apart from session variables):

```sql
SET @migrateGrantee = CONCAT(CHAR(39),'taskcompass_migrate',CHAR(39),'@',CHAR(39),'127.0.0.1',CHAR(39));
SET @runtimeGrantee = CONCAT(CHAR(39),'taskcompass_app',CHAR(39),'@',CHAR(39),'127.0.0.1',CHAR(39));
SET @diagnosticGrantee = CONCAT(CHAR(39),'taskcompass_dev',CHAR(39),'@',CHAR(39),'127.0.0.1',CHAR(39));
SELECT GRANTEE,PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.USER_PRIVILEGES
   WHERE GRANTEE IN (@migrateGrantee,@runtimeGrantee,@diagnosticGrantee);
SELECT GRANTEE,TABLE_SCHEMA,PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.SCHEMA_PRIVILEGES
   WHERE GRANTEE IN (@migrateGrantee,@runtimeGrantee,@diagnosticGrantee);
SELECT GRANTEE,TABLE_SCHEMA,TABLE_NAME,PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.TABLE_PRIVILEGES
   WHERE GRANTEE IN (@migrateGrantee,@runtimeGrantee,@diagnosticGrantee);
SELECT GRANTEE,TABLE_SCHEMA,TABLE_NAME,COLUMN_NAME,PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.COLUMN_PRIVILEGES
   WHERE GRANTEE IN (@migrateGrantee,@runtimeGrantee,@diagnosticGrantee);
SELECT Host,User,Role FROM mysql.roles_mapping
   WHERE User IN ('taskcompass_dev','taskcompass_migrate','taskcompass_app');
```

Expect no global privileges other than possible USAGE, no grant options/extra
explicit roles, diagnostic SELECT unchanged, runtime's nine table-privilege rows
(3 Task, 2 CommandRecord, 2 LocalChange, 1 Dataset, 1 SchemaMigration), and migrator
read-only after revocation. Respect actual server table-name case; do not silently
change physical schema names. On 10.11+ additionally review `SHOW GRANTS FOR PUBLIC;`
and transitive roles without exposing account authentication hashes; skip PUBLIC
on 10.5. Unknown/unreadable effective policy is a stop condition.

#### G. Run the persistent desktop as the runtime user

```bash
set +x
export TASKCOMPASS_DB_HOST=127.0.0.1 TASKCOMPASS_DB_PORT=3306
export TASKCOMPASS_DB_NAME=taskcompass_dev TASKCOMPASS_DB_USER=taskcompass_app
export TASKCOMPASS_DB_ISOLATED=1
read -r -s -p 'Runtime account password: ' TASKCOMPASS_DB_PASSWORD
printf '\n'
export TASKCOMPASS_DB_PASSWORD
runtimeExitCode=0
./build/TaskCompassMigrate --verify && ./build/TaskCompass || runtimeExitCode=$?
unset TASKCOMPASS_DB_PASSWORD
printf 'Runtime verification/launch exit code: %s\n' "$runtimeExitCode"
```

No separate integration env script is needed here; TEST variables do not configure
the GUI. The old dev-environment.sh defaults to the diagnostic account, so avoid
that password/user ambiguity by using the explicit runtime settings above. The
old dev-database.sh test intentionally rejects the now-nonempty schema; use schema
verification rather than weakening its empty-database check.

### Stage 2A persistent GUI acceptance checklist

Use clearly labeled disposable validation tasks, wait for each commit to finish,
and record actual results. Do not mistake fixture prototype behavior for these
checks; launch TaskCompass, not TaskCompassPrototype.

| Check | Expected behavior/evidence |
| --- | --- |
| Initial load/capture | Empty real task list initially; create two tasks with title and notes, including UTF-8/quotes. Reload shows committed records, no fixtures. |
| Edit/sort/selection | Edit title and notes on a selected/sorted row; the same UUID remains selected when visible. Header sorts/filter changes do not mutate records. |
| Completion/filter/reopen | Completion removes task from default active view; Show Completed reveals it. Explicit Reopen restores active and clears completedAt. |
| Undo/redo creation | Undo hides the task through a tombstone; redo restores its original UUID and fields. |
| Undo/redo edit | Restore exact prior title/notes; redo reapplies. |
| Undo/redo complete/reopen | Lifecycle and prior completedAt restored consistently; updatedAt/revision advance, createdAt stays unchanged. |
| History branching/cancel | A new committed edit after undo discards redo; a cancelled edit does not. |
| Reload/restart | Close/relaunch preserves UUIDs, fields, completion timestamps and durable receipts. Session undo intentionally clears on restart; completed tasks still hide by default. |
| Error handling | Invalid title cannot commit. Incorrect runtime credentials produce a clear connection error rather than fixtures or uncommitted rows; do not stop the shared MariaDB service or change unrelated users to simulate failure. |
| Runtime privilege boundary | Metadata matches the approved grants; no destructive CREATE/DROP permission probe is required. CLI --apply with runtime credentials must reject before connecting/writing. |

Read-only verification can use the unchanged diagnostic account:

```sql
SELECT id,title,lifecycleStatus,createdAt,updatedAt,completedAt,deletedAt,localRevision
   FROM taskcompass_dev.Task ORDER BY id;
SELECT COUNT(*) FROM taskcompass_dev.CommandRecord;
SELECT COUNT(*) FROM taskcompass_dev.LocalChange;
```

Record IDs/timestamps/counts needed for review; avoid publishing private task notes.
Receipts/change counts should advance together for committed mutations, including
compensations. Do not physically delete validation records/history as test cleanup.

### Stage 2A failure/backup strategy and exclusions

Preflight must establish no existing application tables/data before migration.
CREATE DDL can partially commit; neither the CLI nor this plan automatically rolls
it back. A failure retains accounts/objects/evidence, stops the workflow and requires
inspection plus a new concrete recovery approval. Do not rerun account creation,
insert successful migration metadata manually, alter fingerprints or grant broader
rights to force success. No DROP taskcompass_dev statement is authorized here.
The protected empty-baseline dump and recorded charset/collation establish the
original state; any development restore or partial-table removal needs separate
target-specific approval. Real data loss recovery must not rely solely on an empty
baseline test; complete Stage 2B below before considering the milestone accepted.

Stage 2A approval excludes scratch restoration, any development restore, account
password replacement, unexpected-resource reuse, cleanup/purge and Phase 1.2.
It does not include Windows success by inference. On Windows/UCRT64 later validate
with matching Qt/compiler/connector, build-ucrt64 executables and MariaDB 13.0.2;
store dumps using --result-file and a private Windows directory/ACL, not an assumed
Unix umask. Record server/Qt/compiler versions, runtime/plugin deployment, live
integration, GUI, restart and recovery results independently. Linux evidence alone
cannot close the Windows gate.

### Stage 2B restore rehearsal — prepared procedure, later exact-file approval

After successful GUI checks, close TaskCompass and stop all TaskCompass writers.
Take another private dump with the same reviewed options, naming it
`after-gui.sql` instead of `before-migration.sql`. Capture exit status/SHA-256,
pre-backup schema verification and one dataset UUID. Preserve all five tables and
history. Inspect the exact dump before requesting restore approval: no account
statements, source USE/CREATE DATABASE, DROP statements, unrelated schema references
or stored code. Logical dumps may contain session FK/SQL-mode save/restore commands;
review those explicitly, never disable checks globally. A dump is executable SQL.

Request separate approval identifying that file's hash, the fresh empty
**taskcompass_integration** restore target, account taskcompass_test and exact
command below. Recheck identity, emptiness and current grants first. Do not reset
an existing target or restore development in place. If the scratch schema is no
longer empty, stop and request specific recovery/reuse approval. No automatic reuse
is inferred from the earlier successful suite.

Proposed later command (not part of Stage 2A approval, do not execute now):

```bash
mariadb --no-defaults --protocol=TCP --host=127.0.0.1 --port=3306 \
   --user=taskcompass_test --password --default-character-set=utf8mb4 \
   taskcompass_integration < "$taskCompassBackupDirectory/after-gui.sql"
```

Use a matching MariaDB client and stop on import error; no --force. A partial
restore leaves evidence and does not authorize cleanup/retry. The current test
account's CREATE/INSERT/SELECT privileges should suffice for the selected dump
options; verify instead of adding rights. This minimal restore path is not yet
validated. Server-version-specific dump syntax/fingerprint changes can require
review; do not silently rewrite metadata. Cross-version restore is not promised.

After an approved import, run `TaskCompassMigrate --verify --integration` with the
explicit TEST account settings/password and compare its dataset UUID to development.
Read/compare **all five tables**, ordered by their primary keys, between the closed
source snapshot and restored target (including tombstones, receipt snapshots and
change payloads); record equal row counts and deterministic exported-data hashes,
not private notes. `SELECT * FROM Dataset ORDER BY id`, Task/CommandRecord/LocalChange
ordered by id, and SchemaMigration ordered by version cover the full schema. Client
batch escaping/UTF-8 settings must match on both exports. Zero exit statuses and
matching rows/IDs/timestamps/history plus schema verification are the acceptance
criteria, not merely successful SQL import.

Do not run MariaDbIntegrationTests on the restored scratch dataset: it intentionally
refuses any pre-existing tables. Preserve the restored evidence; any subsequent
integration-table cleanup requires an explicit separate approval with concrete
qualified SQL. Do not modify the restored dataset via the production GUI (which
correctly only accepts the development runtime account). Record Stage 2B results
and Windows evidence before final owner acceptance of Phase 1.1.

### Validation of this preparation and current outstanding work

Only documentation changed: `docs/milestones/Phase1.1.md`, `README.md`,
`docs/Building.md` and `docs/TestingPhase1.1.md`. Existing uncommitted helper/build/walkthrough changes remain preserved.
No new source, SQL migration, live server query, dump, provisioning, privileges or
GUI mutation was executed by the agent. No dependency install, staging, commit, merge, tag or push.
Current checks are documentation/source agreement, CLI help, migration SHA-256,
Markdown links/fences and diff whitespace. Application build/CTest were not rerun
because source/test behavior is unchanged. Earlier database-free results remain
eight passed/one skipped; the new Stage 1 result is separately owner-reported.

Pending before live Stage 2A: explicit approval of the exact actions above, target
platform/server confirmation, authoritative preflight/grant evidence and empty-
baseline backup. Pending final acceptance: real GUI/undo/restart, populated restore
rehearsal and Linux/Windows validation with actual evidence. No Phase 1.2, commit,
merge, tag or push is authorized.

Stage 2 preparation checks executed (database-free):

```bash
command -v mariadb-dump
mariadb-dump --no-defaults --help
python3 docs/diagrams/GenerateDatabaseSchema.py --check
sha256sum migrations/mariadb/001_PersistentTaskFoundation.sql
git diff --check
```

Results: dump executable/help available (10.5.29 client); diagram/schema checks
passed; migration hash matched; Markdown local-reference/fence checks and diff
whitespace passed. No fresh application CTest run, GUI run or live SQL result is
claimed in this preparation. All Stage 2 SQL/backup/restore commands above remain
proposals; the owner-reported Stage 1 exit-0 transcript is the new live evidence.

## External review safeguards — 2026-11-13

The owner supplied ChatGPT's recommendation for conditional Stage 2A approval.
All five safeguards match the proposed procedure above: confirm the platform and
live server version; verify the development schema has no unexpected contents;
create and verify a private baseline backup before migration; execute only the
documented scope and stop on unexpected results; record actual commands, results
and failures here. A backup must have a successful exit status, nonempty reviewed
output and a recorded checksum; this does not replace the later restore rehearsal.

This supplied recommendation is recorded as review feedback, not explicit owner
execution authorization. No live SQL, backup, provisioning or migration was run.
No source or SQL changes are needed for these safeguards. Reviewed the Stage 2A
preflight, backup, approval and outstanding-work sections for consistency; this
documentation-only update does not require a new application build or CTest run.

### Approval Requested — confirm Stage 2A execution

Approve the exact Stage 2A operations documented above on the Linux development
host, subject to these safeguards. Preflight must establish the actual server
version and empty schema; the Stage 1 platform/server evidence must also be
confirmed before migration. Existing-account conflicts, unexpected privileges or
contents, backup failure and migration failure require stopping for review.
Stage 2B restore operations and failure cleanup remain separately gated.

## Owner-operated Stage 2A testing instructions — 2026-11-13

The owner requested testing instructions. Stage 2A is ready for controlled
execution of the proposed procedure, not yet live-validated. No agent execution
approval is inferred from this request. Follow the Stage 2A subsections A–G in
order: confirm Linux/server and Stage 1 evidence; read-only preflight; private
baseline backup; fresh migration account/grants; apply and verify migration;
runtime provisioning and migrator write revocation; effective privilege review;
runtime verification and persistent GUI launch. Stop on any unexpected result.

Before database setup, configure/build and run database-free CTest using section 1
of docs/TestingPhase1.1.md, with TASKCOMPASS_ENABLE_DB_TESTS=OFF. The earlier Stage 1
integration helpers do not provision Stage 2A runtime or migration accounts.
After setup, execute the persistent GUI acceptance checklist above, including all
undo/redo operations and close/restart. Relaunch with the runtime environment and
password still configured, or repeat subsection G (which clears the password
after the process exits). Report platform/server, commands, exit codes and each
check's actual result without credentials. Stage 2B remains separate.

Documentation validation: reviewed the operator guide and Stage 2A commands for
agreement; git diff --check passed. No live SQL, backup, build, CTest or GUI checks
were executed during this documentation-only clarification.

## Owner-selected backup path — 2026-11-13

Updated subsection B's current command to use
$HOME/temp/TaskCompassBackups/Phase1.1-<timestamp> and derive the parent directory
with dirname, as requested by the owner. The earlier proposed location was
$HOME/TaskCompassBackups/Phase1.1-<timestamp>. Exclusive leaf creation and private
umask remain unchanged. Added immediate exit-status capture, nonempty-file check,
checksum and inspection commands, and explained normal silent output.

Owner-reported evidence: the revised dump command prompted for a password and
returned to the shell without a displayed error. Its exit status, file contents
and checksum have not been supplied; backup verification remains pending. No
agent database access, dump or migration was performed. Only this milestone file
changed in this step. Validation: subsection B Bash blocks passed bash -n via
stdin without execution; git diff --check passed. Application tests were not rerun
because this is a documentation-only command clarification.

## Owner-reported Stage 2A connection failure — 2026-11-13

The owner ran TaskCompassMigrate --apply and received:
`Cannot open MariaDB. Check service/TCP availability, QMYSQL, account, password and grants.`
The owner also reports a successful root connection. That does not establish TCP
access or migration-account authentication: root may use a Unix socket. The
reported error is emitted when QSqlDatabase::open fails, before this connection
executes migration SQL; verification after && was not reached. This is not a
successful migration result. No agent live SQL or account changes were performed.

Next owner diagnostic (read-only, no password in arguments):

```bash
mariadb --no-defaults --protocol=TCP --host=127.0.0.1 --port=3306 \
   --user=taskcompass_migrate --password taskcompass_dev
```

Use the new migration-account password. If connected, run
`SELECT VERSION(), DATABASE(), CURRENT_USER();` and quit. Record any client error
number/message without credentials; distinguish TCP/service (2002/2003),
authentication/host matching (1045), database access (1044) and missing database
(1049) before proposing a correction. If this succeeds but Qt fails, inspect the
explicit subsection D environment/password setup and QMYSQL loading next. Do not
change grants, passwords or retry live migration as an automatic repair.

Validation: reviewed subsection D and the connection-open error path; documentation
whitespace check passed. Application tests not rerun for this documentation-only
failure record. Root cause and live migration/verification remain unresolved.

## Owner-reported Stage 2A migration success — 2026-11-13

After the connection-open failure, the owner successfully authenticated over TCP
as taskcompass_migrate using the manual client. The owner suspected a mistyped
environment-variable password and re-entered it; the exact original cause is not
independently proven. The subsequent owner-executed command was:

```bash
build/TaskCompassMigrate --apply && ./build/TaskCompassMigrate --verify || migrationExitCode=$?
```

Reported output:

```text
Schema 1 verified; dataset 21be9da7-b767-4578-b972-cc9a35122ddb
Schema 1 verified; dataset 21be9da7-b767-4578-b972-cc9a35122ddb
```

Both apply and the chained verification report success with the same dataset UUID.
This is owner-reported evidence, not agent execution. A separate numeric exit-code
transcript was not supplied. Backup exit status/checksum, actual server version,
platform evidence and final account privileges remain to be recorded; do not infer
those checks passed from migration output. Earlier failure evidence is retained.

Next: subsection E runtime-account provisioning and migrator write revocation,
then F effective privilege checks and G runtime verification/GUI launch. Do not
repeat account creation if already performed; inspect unexpected existing accounts.
No GUI persistence, undo/redo, restart or restore success is claimed yet. Agent
live SQL, account changes, builds and tests were not executed for this report.
Only this milestone document changed; git diff --check passed. Phase 1.1 remains
incomplete, and Stage 2B restoration remains separately gated.

## Owner-reported Stage 2A final privilege checks — 2026-11-13

The owner supplied results of subsection F's five metadata queries after step E:

| Query | Owner-reported result | Assessment |
| --- | --- | --- |
| USER_PRIVILEGES filtered to the three accounts | USAGE only, IS_GRANTABLE=NO for app, diagnostic and migrator | Matches; no direct global privileges. |
| SCHEMA_PRIVILEGES filtered to the three accounts | Diagnostic and migrator each SELECT on taskcompass\\_dev, IS_GRANTABLE=NO; runtime absent | Matches; schema wildcard underscore is escaped intentionally. |
| TABLE_PRIVILEGES filtered to the three accounts | Runtime: Task SELECT/INSERT/UPDATE; CommandRecord SELECT/INSERT; LocalChange SELECT/INSERT; Dataset SELECT; SchemaMigration SELECT; nine rows, all IS_GRANTABLE=NO | Matches the exact runtime grant set; migrator CREATE/INSERT grants are absent. |
| COLUMN_PRIVILEGES filtered to the three accounts | Empty | Matches. |
| mysql.roles_mapping filtered to the three accounts | Empty | Matches; no explicit role assignments reported. |

These are owner-executed query results, not agent live queries. They establish the
expected direct privilege separation and migration write revocation. PUBLIC
inspection remains version-dependent: skip on MariaDB 10.5; on 10.11+ review
SHOW GRANTS FOR PUBLIC and any inherited roles before declaring effective grants
fully validated. This transcript does not itself identify the current server
version. No new account/grant changes or destructive permission probes are needed
based on these results.

The owner earlier reported a printed migrationExitCode of 1 after the successful
retry. That variable may retain the prior failure because the retry only assigns
it on failure. Both schema verification messages remain successful output; an
independently captured fresh verification exit code was suggested but not supplied.
Do not reinterpret the stale shell variable as a proven new migration failure.

Next: subsection G runtime-account schema verification and GUI acceptance checks,
after any applicable PUBLIC review. Preserve remaining platform/server, backup,
restart/undo, Windows and Stage 2B restore evidence requirements. Only this
milestone document changed; git diff --check passed. Application tests were not
rerun for this documentation-only evidence update; no agent live SQL was executed.

## Owner-reported Stage 2A GUI acceptance — 2026-11-13

Owner report: “G checked out, including the persistent GUI acceptance checklist.”
Recorded as owner-executed success for subsection G runtime verification/launch
and the complete Stage 2A checklist: task capture and editing; sorting/selection;
completion, completed filtering and reopening; undo/redo for creation, editing,
completion and reopening; redo branching/cancel behavior; reload/restart
persistence with session-only undo; validation/error handling and the runtime
privilege boundary. The earlier supplied step F metadata independently documents
the expected direct grants. No additional detailed per-action logs or fresh exit
code transcript were supplied; this is an owner acceptance report, not an
agent-observed GUI run or new automated test result.

The reported validation is associated with dataset
21be9da7-b767-4578-b972-cc9a35122ddb from the preceding development migration.
Do not infer independent Windows success from this report. Record exact current
platform/server metadata, Stage 1 platform and baseline-backup verification when
available; apply the version-dependent PUBLIC review if relevant.

Remaining milestone work: separately approved Stage 2B populated backup/restore
rehearsal, independently evidenced Linux/Windows Phase 1.1 validation, and final
owner acceptance. The existing Stage 2B proposal is not execution authorization;
review the exact populated backup artifact and fresh isolated destination before
requesting live restore approval. No cleanup of development tasks/history or
integration contents is authorized by the GUI acceptance report. No Phase 1.2.

Files changed in this step: docs/milestones/Phase1.1.md only. Validation:
git diff --check passed. Builds/CTest were not rerun for this documentation-only
owner-result update. No agent live SQL, backup, restore, GUI run or account change
was performed; no commit, merge, tag or push.

## Stage 2B operator clarification — 2026-11-14

Added section 10 to [TestingPhase1.1.md](../TestingPhase1.1.md) with six explicitly
named steps: close writers/verify source, populated backup, destination checks and
exact-file approval, approved import, schema/data comparison, and evidence
preservation. Included a complete after-gui.sql backup command using the owner's
preferred private path and unchanged reviewed dump options. A fresh timestamped
directory avoids the exclusive mkdir failure when reusing an existing directory;
explained that only changing the filename also works when omitting directory
creation and ensuring the new output does not exist. Warned that result-file can
overwrite an existing file. Preserved the original empty-baseline backup.

This is clarification of the existing proposal, not restore authorization or
executed recovery evidence. No live database, backup, migration, account or import
commands were executed. Exact-file inspection, isolated-target preflight, restore
approval and all Stage 2B results remain pending. Source comparison requires
TaskCompass writers to remain stopped throughout the rehearsal.

Files changed: docs/TestingPhase1.1.md and this milestone. Validation: new Bash
blocks passed bash -n through stdin without execution; local guide/milestone links
resolve; Markdown fences are balanced; git diff --check passed. No application
build/CTest rerun because only operator documentation changed. No new dependencies,
source/schema changes, commit or push.

## Owner-reported Stage 2B empty-target checks — 2026-11-14

The owner reports all four administrator catalog queries below returned Empty set:

```sql
SELECT TABLE_NAME,TABLE_TYPE FROM information_schema.TABLES
   WHERE TABLE_SCHEMA='taskcompass_integration';
SELECT ROUTINE_NAME FROM information_schema.ROUTINES
   WHERE ROUTINE_SCHEMA='taskcompass_integration';
SELECT TRIGGER_NAME FROM information_schema.TRIGGERS
   WHERE TRIGGER_SCHEMA='taskcompass_integration';
SELECT EVENT_NAME FROM information_schema.EVENTS
   WHERE EVENT_SCHEMA='taskcompass_integration';
```

The preceding owner SHOW DATABASES transcript includes taskcompass_integration.
Together these establish owner-reported existence and absence of tables/views,
routines, triggers and events at the time checked. This does not establish the
restore account's effective privileges, server identity/version or backup validity.
taskcompass_migrate is an account, not the restore database; the restore source is
taskcompass_dev and destination is taskcompass_integration using taskcompass_test.

Remaining before exact-file restore approval: record populated backup path/hash,
successful dump and source verification exit codes/dataset UUID, private dump
inspection, current server identity/version and destination-account/grant checks.
Keep writers closed and recheck target emptiness if other activity intervenes.
No restore, cleanup, schema changes or agent database access occurred in this step.
Only this milestone file changed; git diff --check passed. Application build/tests
were not rerun for this documentation-only owner-evidence update.

## Owner-reported populated backup artifact — 2026-11-14

Owner supplied the following Stage 2B backup evidence:

- Path: /home/justin/temp/TaskCompassBackups/Phase1.1-20261114-003228/after-gui.sql.
- SHA-256: 5feb67bd973c094e68a07eeb6b02566da319a8746e31d53eba92abe605e450ae.
- test -s returned exit status 0 (nonempty file).
- less opened the file successfully (readable file).

These are owner-reported results; the agent has not opened this private backup or
queried a live database. Opening less is not evidence that the SQL contents were
reviewed. Original dump exit status, confirmation of the required private content
inspection and remaining server/account preflight evidence are still outstanding.
Do not substitute the nonempty-file exit status for the original dump exit status.
Do not rerun the dump over this artifact to recover a lost exit code; if needed,
make a new uniquely named backup and record its own status/hash for review.

No restore authorization is inferred. No source/schema change, live operation or
test execution occurred. Only this milestone file changed; git diff --check passed.
Build/CTest were not rerun because this update only records owner evidence.

### Populated dump exit status unavailable — 2026-11-14

The owner confirmed the original populated dump did not capture its exit status.
Its transcript shows a password prompt and no displayed error, but that does not
prove exit 0. Later commands have replaced the shell's original status. Preserve
the existing artifact/hash as historical evidence. Recommended next action: repeat
the same backup command with a freshly evaluated timestamp directory, immediately
capture the subshell status, and stop on nonzero. Exclusive mkdir must remain in
place to prevent overwrite. Record a new nonempty check and SHA-256 for the new
artifact; privately review it and use that exact file for subsequent approval.
Keep TaskCompass writers closed. No restore or agent live operation was performed.
Only this milestone changed; git diff --check passed. No build/tests rerun for this
documentation-only clarification.

## Owner-reported replacement populated backup — 2026-11-14

The owner repeated the reviewed dump command in a fresh timestamped directory,
captured the subshell exit status immediately, and supplied:

- Path: /home/justin/temp/TaskCompassBackups/Phase1.1-20261114-015008/after-gui.sql.
- SHA-256: f7aa21d4404cc706701cc358e2c2309dddc6c1854a0a7c522c4527a74bb4799c.
- Backup exit code: 0.
- Nonempty-file check exit code: 0.

This supersedes the earlier artifact as the proposed Stage 2B restore candidate;
the prior artifact and unknown-status history remain preserved. Results are
owner-reported, not agent-executed. Private SQL-content inspection, source
verification/server/account evidence and exact-file restore approval remain
pending; no restore success or authorization is inferred from a successful dump.
Next inspect this new file privately for the five expected tables and data and
absence of DROP, account changes, USE/CREATE DATABASE or unrelated schema content.
Keep writers stopped and preserve the candidate unchanged after hashing.

Only this milestone document changed. git diff --check passed; application
build/tests were not rerun for a documentation-only evidence update. No agent
backup, database access, migration, restore, account change, commit or push.

## Owner-reported populated backup inspection — 2026-11-14

The owner inspected the current candidate
/home/justin/temp/TaskCompassBackups/Phase1.1-20261114-015008/after-gui.sql
(SHA-256 f7aa21d4404cc706701cc358e2c2309dddc6c1854a0a7c522c4527a74bb4799c).
Reported tables: CommandRecord, Dataset, LocalChange, SchemaMigration and Task;
no DROP, account changes, USE/CREATE DATABASE or other unexpected content seen.
Record this as owner-reported content inspection; the agent has not read the
private dump. Previous dump exit 0 and nonempty-file exit 0 remain applicable.

Next read-only owner check: connect explicitly over TCP as taskcompass_test to
taskcompass_integration and run SELECT VERSION(), DATABASE(), CURRENT_USER();
SHOW TABLES;. This confirms server version, authenticated account, selected target
and current table emptiness without importing anything. Effective privilege review,
source schema verification and exact-file restore approval remain prerequisites;
the earlier administrator empty-catalog evidence is preserved. No restore is
authorized or executed by this result. Keep writers stopped and the file unchanged.

Only this milestone file changed; git diff --check passed. No application tests
rerun for this documentation-only update; no agent live database operations.

## Test-account password reset procedure — 2026-11-14

The owner reports losing the taskcompass_test password and requested instructions
for setting it. Scope: owner-operated password-only reset of the existing
taskcompass_test@127.0.0.1 account. No grant changes, database reset, recreation or
restore is included. No agent live execution is authorized or performed here.

Open a history-disabled administrator session on Linux:

```bash
sudo env MYSQL_HISTFILE=/dev/null MARIADB_HISTFILE=/dev/null \
   mariadb --no-defaults --protocol=SOCKET --user=root
```

Confirm the intended existing account (stop if absent or unexpected):

```sql
SELECT User,Host FROM mysql.user WHERE User='taskcompass_test';
```

Expected existing account: taskcompass_test@127.0.0.1. Generate a distinct strong
password privately in a password manager, for example 32+ random letters/digits
to avoid SQL quote/backslash escaping problems. Save it there. Replace the
placeholder only in the private history-disabled SQL session, not in this file:

```sql
ALTER USER 'taskcompass_test'@'127.0.0.1'
   IDENTIFIED BY '<new-strong-test-password>';
```

Do not use the literal placeholder, capture the session, put this password in a
shell command/file, or share it. ALTER USER changes the credential, preserving
existing grants and database contents. Other clients using the old credential
will need the new password on reconnect. No FLUSH PRIVILEGES is required. On any
error stop; do not drop/recreate the account or broaden privileges. There is no
automatic credential rollback; a further reset requires an explicit decision.

Exit with quit, then test the new credential at a hidden prompt:

```bash
mariadb --no-defaults --protocol=TCP --host=127.0.0.1 --port=3306 \
   --user=taskcompass_test --password taskcompass_integration
```

```sql
SELECT VERSION(),DATABASE(),CURRENT_USER();
SHOW TABLES;
quit
```

Expected target/account: taskcompass_integration and taskcompass_test@127.0.0.1;
no tables. Report success/error without credentials. Reset and login results are
pending owner execution. Stage 2B exact-file restore remains separately gated.
Files changed: this milestone only. Documentation whitespace check passed; no
application build/tests rerun because application behavior did not change.

## Stage 2B resume checkpoint — 2026-11-14

The owner requested help locating the next step after losing track of progress.
Reviewed the latest milestone entries and added an explicit B3 resume pointer to
section 10 of docs/TestingPhase1.1.md. Completed owner-reported evidence: Stage 2A
GUI checklist, populated backup exit/nonempty checks, chosen artifact SHA-256,
private dump inspection and four administrator empty-target catalog queries.
Pending report: test-account password reset/login success, server/account identity
and current effective privileges, source verification and exact-file restore
approval. No restore execution or completion has been reported.

Next owner command is the read-only TCP login/identity check from the password
reset procedure, not another account reset if already completed and not a new
backup merely to resume. Keep TaskCompass writers closed; if source data changed
since the snapshot, review/recreate a fresh snapshot for exact comparisons without
overwriting this one. The documented restore target remains only
taskcompass_integration, never taskcompass_dev.

Files changed: guide and milestone only. git diff --check passed; no application
build/tests rerun for documentation-only navigation changes. No live SQL, account
change, backup, restore, commit or push executed by the agent.

## Stage 2B destination login and concrete import checkpoint — 2026-11-14

Owner reports successful taskcompass_test login and an empty database check.
Exact VERSION/DATABASE/CURRENT_USER output and current grant metadata were not
supplied; no new server-version or effective-privilege evidence is inferred.
Added the exact selected artifact's hash-check/import/status commands directly
to B4 of docs/TestingPhase1.1.md. No source/schema changes or live execution.

### Approval Requested — exact Stage 2B import

Approve owner-operated import of
/home/justin/temp/TaskCompassBackups/Phase1.1-20261114-015008/after-gui.sql,
SHA-256 f7aa21d4404cc706701cc358e2c2309dddc6c1854a0a7c522c4527a74bb4799c,
into the confirmed empty taskcompass_integration on 127.0.0.1:3306 using
taskcompass_test@127.0.0.1. The exact command is now in B4 and the earlier Stage 2B
proposal. Require unchanged hash, intended identity and reviewed restricted grants;
keep source writers closed for the later comparison. This creates/populates five
test tables, preserving source taskcompass_dev and the private backup. Capture
exit status immediately. Stop on any error: DDL/data may partially commit; retain
evidence and request separately reviewed recovery instead of retry/reset/drop.
No development restore, cleanup, provisioning or privilege changes are included.
Approval and import result remain pending; requesting commands is not recorded
as completed restore or blanket agent live-operation authorization.

Validation: guide Bash blocks passed bash -n without execution; git diff --check
passed. Only guide/milestone changed; no application build/CTest rerun for this
documentation-only clarification. No agent live SQL, restore, commit or push.

## Owner approval — exact Stage 2B import — 2026-11-14

The owner reconfirmed SHA-256
f7aa21d4404cc706701cc358e2c2309dddc6c1854a0a7c522c4527a74bb4799c for
/home/justin/temp/TaskCompassBackups/Phase1.1-20261114-015008/after-gui.sql,
then explicitly answered “Yes” to approval of restoring this file into
taskcompass_integration using taskcompass_test. This approves the exact
owner-operated B4 import on 127.0.0.1:3306, creating/populating the five test
tables while preserving taskcompass_dev. Keep writers closed; capture the import
exit status immediately. Approval excludes cleanup/reset, account/grant changes,
development restoration, or repeated imports after failure. On error, stop and
preserve partial state for separately reviewed recovery. B5 verification and data
comparison are still required; approval is not evidence of import success.

Import result pending owner execution. No agent live SQL or restore was performed.
Only this milestone document changed; git diff --check passed. Application tests
not rerun for this documentation-only approval record. No commit or push.

## Owner-reported Stage 2B import success — 2026-11-14

Following explicit exact-file approval, the owner ran:

```bash
mariadb --no-defaults --protocol=TCP --host=127.0.0.1 --port=3306 \
   --user=taskcompass_test --password --default-character-set=utf8mb4 \
   taskcompass_integration \
   < /home/justin/temp/TaskCompassBackups/Phase1.1-20261114-015008/after-gui.sql
restoreExitCode=$?
printf 'Restore exit code: %s\n' "$restoreExitCode"
```

Owner-reported result: password prompt, no displayed SQL error, Restore exit code:
0. The approved artifact SHA-256 is
f7aa21d4404cc706701cc358e2c2309dddc6c1854a0a7c522c4527a74bb4799c.
This establishes successful client import, not yet full recovery validation.
Do not repeat the import or run destructive integration tests on restored data.

Next B5: TaskCompassMigrate --verify --integration with explicit TEST environment;
require schema 1 and source dataset 21be9da7-b767-4578-b972-cc9a35122ddb, capture
exit status, then compare counts and ordered exports/hashes of all five tables
against the quiescent development source. Keep TaskCompass writers stopped.
Schema/data comparison and final Stage 2B acceptance remain pending.

This is owner-executed evidence; no agent live database operation was performed.
Only this milestone changed; git diff --check passed. No application build/CTest
rerun for documentation-only evidence recording; no commit or push.

## Owner-reported restored schema verification — 2026-11-14

The owner configured TASKCOMPASS_TEST_DB_* for loopback 3306,
taskcompass_integration/taskcompass_test, then ran:

```bash
build/TaskCompassMigrate --verify --integration
verificationExitCode=$?
printf 'Verification exit code: %s\n' "$verificationExitCode"
unset TASKCOMPASS_TEST_DB_PASSWORD
```

Reported output: Schema 1 verified; dataset
21be9da7-b767-4578-b972-cc9a35122ddb; verification exit code 0.
This matches the development dataset UUID. The repeated hidden password read is
not a separate failure; verification succeeded, and the owner cleared the secret.
This is owner-executed evidence, not agent verification. Together with import exit
0 it establishes restored schema acceptance, but not yet equality of task/history
data. All-five-table comparison remains pending.

Added concrete B5 commands to docs/TestingPhase1.1.md for two private escaped,
primary-key-ordered full exports with counts, using the existing restricted
diagnostic and test accounts. The procedure short-circuits on client failure and
requires equal hashes and cmp exit 0; it performs no database writes or cleanup.
Private exports are placed in a new directory beside the selected backup, outside
the repository. Keep all writers closed; commands do not enforce a server lock.

Files changed: guide and milestone. Guide Bash syntax and git diff --check passed;
no live export/database operation was executed by the agent. Application build/
CTest not rerun for documentation-only commands. Stage 2B data equality and final
platform/milestone acceptance remain pending; no commit or push.

## Owner-reported Stage 2B data comparison success — 2026-11-14

The owner ran the B5 comparison: two restricted-account read-only batch exports
with identical UTF-8/batch escaping, row counts and every column of all five tables,
ordered by primary key. Source account was taskcompass_dev on taskcompass_dev;
restore account was taskcompass_test on taskcompass_integration. Commands were
joined with && so export/hash failure would stop comparison. Reported evidence:

```text
6b75033e048fc344d19ee2c33f3a2d4c7b94f3b9a16e47e0889e42a90d214aff  /home/justin/temp/TaskCompassBackups/Phase1.1-20261114-015008/compare-L1SHDF/source.tsv
6b75033e048fc344d19ee2c33f3a2d4c7b94f3b9a16e47e0889e42a90d214aff  /home/justin/temp/TaskCompassBackups/Phase1.1-20261114-015008/compare-L1SHDF/restored.tsv
Comparison exit code: 0
```

Both exported data hashes match, and cmp returned 0. Therefore all five tables'
exported counts and complete ordered contents matched, including dataset metadata,
task IDs/fields/timestamps/tombstones, receipts, change payloads and migration
metadata. This supplements owner-reported import exit 0 and restored schema 1 /
dataset 21be9da7-b767-4578-b972-cc9a35122ddb verification exit 0. The approved backup
artifact/hash and earlier failed/unknown-status attempts remain documented above.

Stage 2B backup/import/schema/data comparison is recorded as owner-verified pass
for this run, not agent-executed or independent Windows validation. Private export
contents were not shared or inspected by the agent. Preserve backup and restored
integration data; do not rerun destructive integration tests, import again or drop
these tables without a separate concrete approval. No development data was reset.

Remaining: exact platform/server metadata and outstanding preflight evidence,
independent Windows Phase 1.1 validation and final owner acceptance. Phase 1.1 is
not marked complete, and Phase 1.2 is not authorized. Files changed: milestone and
operator guide current-status pointer. git diff --check passed; no application
build/tests rerun for documentation-only owner-evidence recording. No agent live
SQL, restore, account change, commit or push.

## Scripted Windows validation preparation — 2026-11-14

### Scope and owner direction

The owner initially deferred Windows testing, then requested simplifying it through
scripts run one step at a time. Implemented validation tooling only, within Phase
1.1. No production functionality, domain/schema changes, dependency installation,
live database access or later milestone work. Existing Linux evidence above is
preserved; it does not authorize or prove Windows live results.

### Files created or modified in this step

- scripts/phase11-validate.sh: Bash runner with build, inspect, integration-setup,
  integration-test, migration-setup, migrate, runtime-setup, gui, backup, restore
  and compare actions. No arguments/help/--print-plan perform no connections.
- scripts/phase11/migration-account.sql: exact migration account/grant review
  template with in-memory-only password placeholder replacement.
- scripts/phase11/runtime-account.sql: exact runtime grants and migrator write
  revocation template; no overwrite/reset/account repair.
- tests/Phase11ValidationScripts.test.sh: mock-only safety and end-to-end helper
  tests. All credentials/client/executable behavior here are fabricated fixtures.
- CMakeLists.txt: register Phase11ValidationScripts in ordinary CTest when Bash
  exists. Live integration registration remains OFF by default.
- docs/WindowsPhase1.1.md: numbered command sequence, password/account explanations,
  script expectations, approval/recovery boundaries and manual checks.
- README.md, docs/Building.md, docs/TestingPhase1.1.md: link the scripted path and
  align current result summaries; manual walkthrough preserved.
- docs/milestones/Phase1.1.md: this report and current navigation pointer. Existing
  uncommitted owner/conversation changes were preserved, not reset or committed.

### Behavior and architecture decisions

Retained all database identities, five-table migration SQL and role boundaries.
No schema change means no schema diagram regeneration is required. The runner
uses the existing TaskCompassMigrate/GUI/integration executables; it is not an
alternate persistence/migration implementation. Integration provisioning delegates
to the existing guarded temp/testdb/setup.sh. Ordinary tests invoke no live clients.

Windows defaults to build-ucrt64, CMAKE_PREFIX_PATH from /ucrt64 and a consistent
compiler/runtime PATH. Client paths with spaces and explicit client/dump overrides
are supported. Native dump result-file paths use cygpath, and password variables
are excluded from MSYS2 automatic environment-path conversion. It does not require
Qt Creator, VS Code, Python, a new framework or new installed packages.

Administrative checks use root over loopback TCP; Linux users with socket-only root
should retain the manual/sudo-socket route rather than change root authentication.
Before mutation the script checks expected schema existence/charset/collation and
absence of tables/views/routines/triggers/events, and rejects pre-existing new
account names on any host. Grant checks require exact direct scope/table rights,
no global write/grant authority, no column grants and no explicit roles. On MariaDB
10.11+ it reuses the existing SHOW GRANTS parser for PUBLIC, accepts unrelated
PUBLIC test/test_% grants, rejects matching write/DDL/grant rights, and conservatively
stops for manual review if PUBLIC inherits roles. No PUBLIC/grant repair occurs.
Final runtime, diagnostic and revoked migrator grants are checked automatically.

Live write/test actions require --approved and typed target-specific confirmation;
no Linux approval is inferred for Windows. Password prompts identify accounts,
new passwords are confirmed twice, and no passwords are written in SQL/config/log
files or argv. Only per-child client environments and existing Qt configuration
variables carry secrets; exit clears them from the runner. Diagnostics redact raw
and SQL-quoted secrets. MariaDB 13 password-option warnings are explained without
disabling TLS verification. Scripts do not log grants containing authentication
hashes or print private task data. No environment/profile files are generated.

New baseline/snapshot directories must be outside the repository and absent.
Linux uses umask 077; Windows uses built-in PowerShell to establish and verify a
current-user-only protected directory ACL before data is written. Failures stop;
actual PowerShell/NTFS behavior remains unverified until Windows execution. Client
and server versions must match the backup identity for the proposed restore path;
cross-server/version restores remain a separate design/review decision.

Migration is explicit and verifies immediately; partial MariaDB DDL is not rolled
back or automatically resumed. Runtime provisioning revokes migrator writes only
after schema verification. GUI verification and launch are scripted; interactive
behavior/restart review is still owner-performed, never inferred from window exit.
Populated backup requires writers closed, saves the source export before/after
and rejects changed snapshots. Restore checks backup/export hashes, exact-hash
confirmation, private dump review confirmation, empty integration target, server
identity, account identity and effective grants before import. It cannot target
development or reset the destination. Failed import stops and preserves partial
state. Comparison verifies schema/dataset and all five tables/counts against both
the saved source snapshot and current development; writes must stay stopped.
Comparison artifacts are not overwritten. No cleanup script is provided.

### Validation commands and actual results

Agent executed on Linux, with no live SQL:

```bash
bash -n scripts/phase11-validate.sh tests/Phase11ValidationScripts.test.sh
bash scripts/phase11-validate.sh --help
bash scripts/phase11-validate.sh migration-setup --print-plan
bash tests/Phase11ValidationScripts.test.sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DTASKCOMPASS_ENABLE_DB_TESTS=OFF
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
python3 docs/diagrams/GenerateDatabaseSchema.py --check
sha256sum migrations/mariadb/001_PersistentTaskFoundation.sql
git diff --check
```

Results: Bash syntax, no-op/plan checks and mock suite passed. Mock coverage:
no-client defaults/previews/unapproved operations; typed confirmation mismatch;
existing accounts/nonempty targets; unexpected direct and PUBLIC privileges;
password redaction and quote/backslash/ampersand SQL escaping; integration test
only after grant/catalog gates; role-specific migration/runtime/GUI configuration;
backup failure/wrong identity/refusal to overwrite evidence; import failure;
checksum tampering; dataset verification and matching/mismatched full exports.
On Linux, fake UCRT64/cygpath/PowerShell dispatch additionally checks that ACL
failure stops before a dump; it does not validate native Windows ACL execution.

CMake configure and build succeeded. Ordinary CTest: **9 passed, 1 skipped, 0
failed** (10 registered); skipped MariaDbIntegrationTests performs no live SQL.
The previous eight-pass total predates registering this mock suite. Follow-up
changes to the runner/tests receive the final focused CTest check below rather
than claiming earlier output validates later edits. Diagram provenance check and
migration checksum remain unchanged; no migration execution occurred.
Markdown links/fences and diff whitespace checks are recorded with the final check.

### Limitations and remaining work

Native Windows build/runtime, real MariaDB 13 grants/provisioning/migrations,
PowerShell ACL enforcement, native client paths/warnings, persistent GUI and
backup/restore are not yet tested with these helpers. Conservatively unsupported
grant/role policies stop instead of being automatically broadened. The runner does
not detect every noncooperating writer or enforce a global server lock. Human
private dump inspection and explicit live approvals remain necessary. It does not
automatically update this milestone; actual owner results must be recorded here.
No step resets existing databases/accounts. A failure may leave partial provisioning,
DDL or private evidence; inspect and propose exact recovery with separate approval.
Full Phase 1.1 acceptance and Phase 1.2 remain unapproved.

### Approval Requested — Windows live stages, after inspection

Safe next owner commands are build and inspect; provide Windows server identity,
existing resources and test/build results before requesting execution of write
stages. Source-tooling authorization does not authorize live Windows changes.
The fresh-target proposals use the exact SQL review templates linked above and
existing milestone Stage 1/2A migration 001, not new or broader grants:

1. Windows Stage 1: if absent, create taskcompass_integration and
   taskcompass_test@127.0.0.1 with SELECT/INSERT/UPDATE/CREATE/DROP/ALTER only on
   escaped taskcompass_integration scope; run the existing opt-in disposable suite
   on an entirely empty test schema. Only its own five integration tables may be
   dropped by approved test cleanup. Refuse unexpected objects/roles/grants.
2. Windows Stage 2A, separately after integration passes: require empty existing
   taskcompass_dev and unchanged diagnostic account; private baseline backup,
   fresh taskcompass_migrate with the exact template CREATE/SELECT and metadata
   INSERT grants, explicit migration 001/verification; fresh taskcompass_app exact
   nine table grants, revoke migrator CREATE/INSERT, final metadata validation and
   owner-operated persistent GUI checklist. No existing account password/grant
   overwrite or development DROP/ALTER/restore/cleanup is proposed.
3. Windows Stage 2B, separately after GUI checks: create/inspect a populated backup,
   identify its exact path/hash and obtain exact-file approval before importing
   into the empty integration target. Verify/compare the restored data afterward.

Each stage stops on unexpected state and preserves evidence. MariaDB DDL can
partially commit; backups do not automatically authorize live recovery. Cleanup,
failed-step retry/reset, privilege repair or development restore requires a new
concrete procedure/approval. No live operations were executed while creating these
scripts; all Windows live approval/results remain pending.

### Final tooling checks and handoff

Final focused validation after the last runner/test edits:

```bash
bash -n scripts/phase11-validate.sh tests/Phase11ValidationScripts.test.sh
ctest --test-dir build --output-on-failure
python3 docs/diagrams/GenerateDatabaseSchema.py --check
sha256sum migrations/mariadb/001_PersistentTaskFoundation.sql
git diff --check
```

Result: 9 passed, 1 skipped, 0 failed in 3.50 seconds. Mock suite includes fake
UCRT64 path conversion/ACL failure and successful dispatch; actual Windows remains
pending. Migration SHA-256 is still
bb901d7580398893b5c04ee263f813b9d05873eb4bae08e0daf377faf73b0ea0;
diagram check reports 5 tables/38 columns/4 SQL FKs/2 logical references unchanged.
New/updated documentation local links and balanced fences passed. Bash code blocks
passed bash -n via line-anchored fence extraction, without executing documentation
commands. An initial naive fence splitter produced two historical here-document
warnings by stopping inside Python string literals containing backticks; corrected
line-anchored extraction verified those existing blocks cleanly. No document code
or live database operation was executed by these syntax checks.

Also updated docs/Roadmap.md's current-status pointer from pre-database review to
validation and linked the Windows route; historical evidence stays in this file.
All changes remain uncommitted. Safe next owner Windows commands: run the guide's
build and inspect steps, report actual results, then approve the appropriate fresh
Stage 1/2A operations. Do not rerun fresh provisioning against existing resources.

## Numbered validation stages and scripts — 2026-11-14

Owner requested numbers so stages/scripts sort in execution order. Added thin
executable entry scripts under scripts/phase11/; each delegates to the existing
runner and executes only its own stage:

```text
01-build.sh
02-inspect.sh
03-integration-setup.sh
04-integration-test.sh
05-migration-setup.sh
06-migrate.sh
07-runtime-setup.sh
08-gui.sh
09-backup.sh
10-restore.sh
11-compare.sh
```

Updated docs/WindowsPhase1.1.md headings/commands to the same 01–11 numbering.
The shared runner now accepts numbered names (01-build), zero-padded stage numbers
(01) and ordinary numbers (1), retains all original names, and prints the stage
number. All safety checks/approval flags/confirmations remain in one implementation;
wrappers cannot skip them. Numbering does not automatically execute subsequent
steps or authorize live writes. Existing Windows approvals remain pending.

Files changed in this step: the eleven new wrapper files,
scripts/phase11-validate.sh, tests/Phase11ValidationScripts.test.sh,
docs/WindowsPhase1.1.md and this milestone. No schema, SQL templates or application
behavior changes. No additional dependencies or architectural conflicts.

Validation executed:

```bash
bash -n scripts/phase11-validate.sh scripts/phase11/*.sh
bash tests/Phase11ValidationScripts.test.sh
ctest --test-dir build --output-on-failure
git diff --check
```

Results: syntax and mock tests passed. Added mock assertions that all eleven
wrappers and numeric aliases produce identical nonconnecting previews, and every
mutating wrapper rejects execution without --approved before a client is invoked.
Guide heading/script/command order, local references and Bash block syntax passed.
CTest: 9 passed, 1 live integration skipped, 0 failed in 3.62 seconds. No fresh C++
build needed because only scripts/docs changed and existing compiled test targets
are unchanged. Native Windows execution remains pending. No live SQL, provisioning,
migration, restore, commit, merge, tag or push executed by the agent.

## Windows stage 01 tooling failures and corrections — 2026-11-14

### Owner-executed evidence

Owner ran scripts/phase11/01-build.sh in UCRT64 on Windows. Reported tools:
CMake 4.4.5, MSYS2 GCC 16.2.0 Rev6, Qt 6.12.0. CMake configured and all 45 build
steps completed, including linking StorageSafetyTests.exe. This establishes
owner-reported successful Windows compilation, not final runtime acceptance.

CTest reported 10 tests: 7 passed, 2 failed, 1 live integration skipped:

- DevDatabaseScripts hit its 30.03-second timeout after multiple PASS messages,
  including unrelated PUBLIC grants and a safe read role. Remaining checks were
  not completed; do not claim that suite passed.
- Phase11ValidationScripts failed after 40.65 seconds with runner line 419:
  `cmp: command not found`, followed by the misleading
  `STOP: Source changed during backup; preserve files and take a fresh quiescent backup.`
  This was a mock-only tooling failure, not evidence of live source changes.
- PrototypeTests, GuiStartup, IntegrationDatabaseScripts, TaskCoreTests,
  MigrationSqlPreview, PersistentDesktopTests and StorageSafetyTests passed.
  GuiStartup is an automated prototype smoke test, not persistent interactive review.

### Corrections and files

- scripts/phase11-validate.sh: remove cmp/diffutils dependency from backup/compare.
  Use SHA-256 equality, already used by the approved comparison procedure. Hash/read
  failures stop with a tooling error, distinct from differing contents. Check
  sha256sum and sort availability before client discovery/password prompts/live SQL.
  No package installation is needed to fix missing cmp.
- CMakeLists.txt: Windows-only 180-second limits for all three Bash mock suites.
  Linux retains 30 seconds for existing suites and 60 seconds for Phase11ValidationScripts.
  MSYS2 process startup overhead is a plausible explanation for the observed timeout;
  raising a finite limit does not waive any assertions or prove remaining checks pass.
- tests/Phase11ValidationScripts.test.sh: regressions for unavailable hash tool
  before any client call; an intentionally unusable cmp that is never invoked;
  hashing failure reported as hashing failure rather than source changes; existing
  positive equality and true mismatch tests retained. Limited-PATH fixtures use
  small wrappers instead of symlinks, avoiding a Windows symlink permission dependency.
- docs/WindowsPhase1.1.md: explain unchanged coreutils requirement, absence of a
  diffutils requirement, Windows timeout limits and why stage 01 must regenerate.
- docs/milestones/Phase1.1.md: this result/failure/correction record and status pointer.

No production code, task schema, migration SQL, database accounts/grants or live
contents were changed. No new dependencies or architecture exceptions introduced.
No installation or Windows command execution was performed by the agent.

### Agent validation on Linux

```bash
bash -n scripts/phase11-validate.sh tests/Phase11ValidationScripts.test.sh
bash tests/Phase11ValidationScripts.test.sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DTASKCOMPASS_ENABLE_DB_TESTS=OFF
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
git diff --check
```

Syntax and mock checks passed. Configure/build passed; final CTest: **9 passed,
1 skipped, 0 failed**, 8.10 seconds. The skipped live suite performed no SQL.
Windows guide local references, balanced fences and Bash command syntax passed.
No live database operation, restore, migration or account change was executed.
No commit, merge, tag or push.

### Next owner step and limitations

Transfer these updated files to Windows and rerun:

```bash
bash scripts/phase11/01-build.sh
```

This regenerates the CMake timeout settings before CTest. No installation of cmp
or diffutils is requested. Share the new test output, including any actual failures
or further timeouts. Do not mark Windows stage 01 or full Phase 1.1 passed until
that rerun succeeds. Live Windows provisioning/migration/restore still require
previously documented distinct approval checkpoints; no new live approval is
requested merely for this database-free rerun.

## Owner-reported Windows stage 01 success — 2026-11-14

Owner reran scripts/phase11/01-build.sh after the tooling corrections on Windows
UCRT64. Reported CMake 4.4.5, MSYS2 GCC 16.2.0 Rev6, Qt 6.12.0. Configure/generate
and build succeeded in C:/Users/justin/repo/TaskCompass/build-ucrt64.

CTest: **9 passed, 1 skipped, 0 failed**, total 139.79 seconds. Specifically:
PrototypeTests 0.42s; GuiStartup 0.67s; DevDatabaseScripts 65.68s;
IntegrationDatabaseScripts 4.78s; Phase11ValidationScripts 66.61s; TaskCoreTests
0.02s; MigrationSqlPreview 0.04s; PersistentDesktopTests 1.05s; StorageSafetyTests
0.43s. MariaDbIntegrationTests skipped (0.04s), as intended for ordinary CTest.
The 180-second Windows limits allowed the previously failing suites to finish;
earlier failures remain preserved above rather than overwritten.

Driver diagnostic reported QIBASE, QSQLITE, QMARIADB, QMYSQL, QODBC and QPSQL;
QMYSQL available yes, loadable yes. Connection intentionally skipped because none
was requested. Runner concluded PASS: build and database-free tests; no live
validation claimed. This establishes owner-reported stage 01 success, not live
MariaDB authentication or the persistent GUI acceptance checklist.

Next owner command, read-only inspection:

```bash
bash scripts/phase11/02-inspect.sh
```

Use the administrator database password when prompted. If client autodiscovery
fails, provide --client with the installed mariadb.exe path. Share server/resource
inspection output without credentials to determine whether fresh provisioning
stages are appropriate. Do not rerun setup blindly if resources already exist.
Windows live stages retain their explicit approval checkpoints. Phase 1.1 remains
incomplete; no Phase 1.2 authorization inferred.

Only this milestone document changed. Agent validation: git diff --check passed;
no application build/tests rerun for this documentation-only owner-result record.
No agent Windows execution, live SQL, provisioning, migration, restore, commit,
merge, tag or push was performed.

## Windows administrator credential unavailable — 2026-11-14

After successful Windows stage 01, the owner reports forgetting the Windows
MariaDB root password. Stage 02 administrator inspection is blocked pending
credential recovery or another already-authorized administrator login. No root
reset, service stop, authentication bypass, reinstall, account/grant change or
live SQL has been performed or authorized by this report. Application/diagnostic/
test passwords are separate and must not be substituted or exposed.

Reviewed existing Installation.md Windows service/password instructions and
MariaDB's official Windows MSI and server-option documentation:
https://mariadb.com/docs/server/server-management/install-and-upgrade-mariadb/installing-mariadb/binary-packages/installing-mariadb-msi-packages-on-windows
https://mariadb.com/docs/server/server-management/starting-and-stopping-mariadb/mariadbd-options
Recovery must preserve the existing instance/data and have a concrete separately
approved account/service procedure; do not reinstall or initialize a new data
folder as a password repair. Do not assume Linux socket-root behavior on Windows.

Next owner step: check the password manager or another existing administrator
session first. If unavailable, collect read-only Windows service metadata in
PowerShell (not SQL or UCRT64):

```powershell
Get-CimInstance Win32_Service |
   Where-Object { $_.PathName -match '(mariadbd|mysqld)(\.exe)?' } |
   Select-Object Name, State, StartName, PathName |
   Format-List
```

This identifies the service executable/configuration and service identity for a
reviewable recovery plan; it does not stop/change the service. Service metadata
has not yet been supplied. Do not share passwords. Exact root-account identity,
instance configuration, downtime scope and recovery approval remain unresolved.
No application source/scripts changed. Only this milestone changed; git diff
--check passed. No build/CTest rerun for a documentation-only blocker record;
previous Windows stage 01 success remains valid. No commit or push.

## Windows MariaDB service identified; recovery proposal — 2026-11-14

Owner supplied Get-CimInstance results: service MariaDB is Running as
NT SERVICE\MariaDB. PathName identifies
C:\Program Files\MariaDB 13.0\bin\mysqld.exe with
--defaults-file=C:\Program Files\MariaDB 13.0\data\my.ini and service name MariaDB.
This is owner-reported service metadata, not proof of the actual configured data
directory or authenticated root-host identity. No credentials were supplied.

Created docs/WindowsMariaDbRecovery.md with concrete PowerShell/native-client/SQL
steps and linked it from docs/WindowsPhase1.1.md. Reviewed official MariaDB
recovery, named-pipe connection, SET PASSWORD and system-variable documentation
(linked in the procedure); MariaDB 13 supports the checked skip_grant_tables and
skip_networking variables. Use FLUSH PRIVILEGES before SET PASSWORD, without
rewriting authentication plugins or changing grants. PowerShell is unavailable in
this Linux environment; commands have not been parsed or executed by PowerShell.
No Windows service, SQL, database files or account was accessed by the agent.

### Approval Requested — owner-operated Windows root recovery

Approve only the documented recovery for the identified service/configuration:
read nonsecret config fields; stop MariaDB cleanly; start the same server/config
in a temporary foreground process with TCP disabled, a dedicated local named pipe,
SQL text logs/binlog disabled and temporary skip-grant-tables; inspect intended
instance/root plugin and stop on unexpected identities/audit/configuration; then:

```sql
FLUSH PRIVILEGES;
SET PASSWORD FOR 'root'@'localhost' = PASSWORD('<new-strong-root-password>');
SHUTDOWN;
```

The placeholder is replaced privately in a history-disabled interactive client,
never in a file/argv/log. Scope is existing password-capable root@localhost only;
no other root-host accounts, application accounts, grants, databases, service
registration or my.ini changes. Brief service downtime affects all clients of
that instance: coordinate/close them first. Recovery bypass is local-only but
requires a trusted machine during that interval. Root/account and service changes
require this separate authorization under AGENTS.md; forgotten credentials and
service inspection are not execution approval.

Restart the unchanged normal service only after the recovery process exits and
test root TCP login/normal authentication/network flags. No reinstallation, new
data directory, database deletion, InnoDB-file removal, copied credentials,
force-kill, ownership repair or broader authentication rewrite is authorized.
Do not access unrelated application data. Normal databases remain in place; no
physical backup of unrelated databases is proposed or performed. This procedure
is not data recovery and does not authorize replacing database files.

On startup failure, stop and preserve errors; if foreground server is fully exited,
restart the unchanged service to restore prior behavior. Before a successful SET
PASSWORD, the old credential is unchanged; afterward there is no known-old-password
rollback. Retain the new password securely and request a revised procedure on any
failure rather than trying additional account changes. Datadir/plugin/include/
audit surprises require review before resetting anything. No live execution or
success is claimed. Existing TaskCompass Windows validation approvals remain separate.

Validation: procedure/guide/milestone Markdown local links and balanced fences,
git diff --check. No application build/CTest rerun for documentation-only recovery
planning. Native Windows/PowerShell/MariaDB 13 recovery remains untested. Files
changed: new recovery guide, Windows validation guide link and this milestone.
No package installation, commit, merge, tag or push.

## Owner-reported Windows stage 02 inspection — 2026-11-14

After reporting administrator access recovered, the owner ran
scripts/phase11/02-inspect.sh. The exact root recovery actions were not supplied;
record successful administrator TCP access, not execution/verification of the
proposed recovery procedure. Inspection reported:

- Server feldria-w10, 13.0.2-MariaDB, TCP 3306.
- Platform MINGW64_NT-10.0-19045 / UCRT64 (the MSYSTEM is UCRT64; uname's MINGW64
  text does not mean the wrong MSYS2 environment).
- Only taskcompass_dev among the two requested database names, utf8mb4 /
  utf8mb4_unicode_ci; no table rows printed by the scoped catalog report.
- Only taskcompass_dev@127.0.0.1 among the requested four accounts.
- Diagnostic direct grants: USAGE, schema SELECT only, IS_GRANTABLE=NO; no other
  account grant rows printed. Escaped backslash display in batch output is not a
  request to rewrite the schema grant.
- PASS: PUBLIC has no write/DDL authority covering either TaskCompass schema.
- Native client password-option notices were explained; no TLS verification flags
  were disabled. This was a read-only report; no automatic overwrite occurred.

This supports fresh Windows integration provisioning as the next stage; it does
not prove every stored-object catalog check, live task operation or normal
post-recovery authentication flags. Before write steps confirm the temporary
recovery server has exited and the normal service is running (skip_grant_tables=0,
normal TCP enabled); do not run tests/provisioning with authentication bypass.
No owner metadata/credentials were independently queried by the agent.

### Approval Requested — Windows stages 03 and 04

Approve only the fresh isolated Windows Stage 1 provisioning and disposable tests
on feldria-w10 / 127.0.0.1:3306. Stage 03 uses the existing exact template:

```sql
SET SESSION sql_mode='NO_BACKSLASH_ESCAPES';
CREATE DATABASE taskcompass_integration
   CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER 'taskcompass_test'@'127.0.0.1'
   IDENTIFIED BY '<distinct-strong-test-password>';
GRANT SELECT, INSERT, UPDATE, CREATE, DROP, ALTER
   ON `taskcompass\_integration`.* TO 'taskcompass_test'@'127.0.0.1';
```

The password is supplied privately to the helper in memory, not edited into a
file. Stage 03 refuses existing database/account names. Stage 04 verifies exact
privileges/PUBLIC and completely empty target, then runs the existing opt-in
MariaDbIntegrationTests. Approved scope would include only its own five disposable
integration tables and known in-process DROP cleanup after empty-start proof, never
development. No application/diagnostic account changes or unrelated data access.
On partial provisioning/DDL/test failure, stop and preserve evidence; no automatic
retry/reset, overwrite or broader grants. Recovery/cleanup needs separate approval.
Stages 05–07 development setup/migration and stage 10 exact-file restore are not
included and retain separate checkpoints.

Only this milestone changed in this step. git diff --check passed. Application
build/CTest not rerun for documentation-only owner-evidence/approval preparation.
No agent live SQL, provisioning, migration, restore, commit or push.

## Owner approval — Windows stages 03–04 — 2026-11-14

The owner explicitly answered “Yes I approve” to the preceding Windows Stage 1
request: fresh taskcompass_integration/taskcompass_test@127.0.0.1 provisioning
and guarded disposable integration tests on feldria-w10 / 127.0.0.1:3306.
This approves only the exact stages 03–04 SQL/procedure/scope recorded above,
including in-process cleanup of the test's own five tables after empty-start proof.
Normal service operation without recovery authentication bypass is a prerequisite.
No development changes, existing-account overwrite, cleanup of pre-existing objects,
failed-step repair/retry, broader grants or Windows stages 05–07/10 are included.

Next owner commands, one at a time:

```bash
bash scripts/phase11/03-integration-setup.sh --approved
bash scripts/phase11/04-integration-test.sh --approved
```

Proceed to 04 only if 03 succeeds; stop and report unexpected state/errors rather
than repeating setup. Stage 03 confirmation: CREATE taskcompass_integration;
passwords: administrator, new test password twice. Stage 04 confirmation:
TEST taskcompass_integration WITH DISPOSABLE RESET; passwords: administrator then
test account. Save the test password securely. Actual provisioning/test results
are pending owner execution; approval is not evidence of success.

Only this milestone changed; git diff --check passed. No application build/tests
rerun for this documentation-only approval record. No agent live SQL, provisioning,
test execution, migration, restore, commit or push.

## Owner-reported Windows stages 03–04 success — 2026-11-14

On feldria-w10 / MariaDB 13.0.2 / 127.0.0.1:3306, the owner executed the approved
scripts in UCRT64. Stage 03 created taskcompass_integration and the dedicated
TCP test account with six scoped privileges. Its output explicitly states no
migrations/tests ran during setup. Two MariaDB password-option warnings appeared;
no connection failure or TLS-verification override was reported.

Stage 04 was initially attempted twice with only TEST taskcompass_integration.
Both stopped with Confirmation mismatch; nothing changed. The successful third
attempt supplied the entire required phrase:
TEST taskcompass_integration WITH DISPOSABLE RESET. These two confirmation
rejections are expected guards, not SQL/test failures, and occurred before the
administrator login/preflight/test invocation.

The successful attempt reported:

```text
PASS: exact direct grants and no explicit roles for taskcompass_test.
PASS: PUBLIC has no write/DDL authority covering either TaskCompass schema.
Running disposable integration tests on taskcompass_integration@127.0.0.1:3306.
PASS: MariaDB migrations/version/checksum/fingerprint, persistence/restart, commands, constraints and atomic rollback. Test schema left empty.
Integration test exit code: 0
```

Record Windows Stage 1/stages 03–04 as owner-verified passes. The test schema was
left empty by the approved test cleanup; do not rerun provisioning. This does not
claim development migration, interactive persistent GUI/restart or Windows restore
success. Client notices remain explanatory output, not grounds to disable TLS.

### Approval Requested — Windows stages 05–07 development foundation

Request separate owner approval for the exact existing Stage 2A procedure on
feldria-w10 / 127.0.0.1:3306 only. Source and concrete SQL are already reviewable in:

- scripts/phase11/migration-account.sql (new migration account and narrow grants).
- migrations/mariadb/001_PersistentTaskFoundation.sql, unchanged SHA-256
  bb901d7580398893b5c04ee263f813b9d05873eb4bae08e0daf377faf73b0ea0
  (full DDL also reproduced earlier in this milestone).
- scripts/phase11/runtime-account.sql (new runtime grants and exact migrator
  CREATE/INSERT revocations).
- docs/WindowsPhase1.1.md stages 05–07 (one-command sequence, prompts and gates).

Scope: stage 05 verifies the existing development database is entirely empty with
utf8mb4/utf8mb4_unicode_ci, the diagnostic account unchanged SELECT-only, no new
account names on any host, and no unexpected PUBLIC policy. It establishes and
verifies private Windows backup ACLs, takes/checksums a nonempty baseline dump,
then creates taskcompass_migrate with CREATE/SELECT on the exact escaped
TaskCompass development scope and CREATE/INSERT only on Dataset/SchemaMigration.
If ACL, backup or provisioning fails, stop and preserve evidence; no silent reuse.

Stage 06 verifies the baseline hash/server identity and fresh schema/grants before
executing migration 001 through the dedicated account, then verifies schema 1 and
a new Windows dataset UUID. MariaDB CREATE DDL can partially commit; failure is
not automatically rolled back, repaired, adopted or resumed. Preserve partial
objects/metadata and request a new concrete recovery plan. Never edit schema
version/checksum/fingerprint or broaden privileges to force success.

Stage 07 verifies migration state, creates taskcompass_app with only Task
SELECT/INSERT/UPDATE, CommandRecord and LocalChange SELECT/INSERT, Dataset and
SchemaMigration SELECT; revokes migrator CREATE and metadata INSERT/CREATE and
verifies the exact final runtime/diagnostic/read-only migrator grants. No existing
account passwords/grants or diagnostic privileges are overwritten. A partial
provision/revoke failure stops and retains state for review.

No DROP/ALTER/development restoration, unrelated database access, existing-account
reset, failure cleanup or additional privileges are included. The private empty
baseline backup preserves evidence of the original state; it does not make DDL
transactional or authorize a live restore. Populated recovery rehearsal/Windows
stage 10 remains a separate later exact-file approval. Keep normal authentication
enabled and other TaskCompass writers closed while migrating. Do not repeat stage
03 or destructively clear databases to fit this procedure.

Provisioning/migration results for Windows stages 05–07 are pending this approval.
No agent live SQL was executed. Only this milestone changed; git diff --check
passed. No application build/CTest rerun for documentation-only owner-result and
approval preparation. No commit, merge, tag or push.

## Owner approval — Windows stages 05–07 — 2026-11-14

Owner explicitly answered “Yes, I approve” to the immediately preceding Windows
stages 05–07 development-foundation request. Approval covers only its exact SQL,
migration 001/hash, backup/preflight, new migration/runtime accounts, narrow grants,
and migrator write revocations on feldria-w10 / 127.0.0.1:3306 as documented above.
Existing diagnostic credentials/SELECT-only privileges remain unchanged. Stage 10
restore, cleanup/reset, unexpected account reuse, failure repair and unrelated
operations remain excluded. Stop on any unexpected result and preserve evidence.

Run stages one at a time, proceeding only after each passes. First owner command:

```bash
baselineDirectory="$HOME/TaskCompassBackups/windows-baseline-$(date +%Y%m%d-%H%M%S)"
bash scripts/phase11/05-migration-setup.sh --approved --backup-dir "$baselineDirectory"
```

Keep this terminal and baselineDirectory value for stage 06. Stage 05 confirmation
is PROVISION taskcompass_migrate; password order is administrator root, existing
diagnostic taskcompass_dev, then new taskcompass_migrate password twice. Save the
new credential privately. No source/file should receive it. If client/dump
locations require overrides, use the guide's dbOptions array with the same paths
as the inspected installation. No live operation or success is inferred from this
approval. All Windows stage 05–07 execution/results remain owner-pending.

Only this milestone changed. git diff --check passed; no application build/tests
rerun for this documentation-only approval record. No agent live SQL, provisioning,
migration, backup, restore, commit or push.

## Windows stage 05 password confirmation stopped before provisioning — 2026-11-14

Owner transcript reports all administrator empty-schema/account/grant/PUBLIC
preflight checks passed on feldria-w10 / MariaDB 13.0.2. Diagnostic authentication
and baseline dump succeeded; nonempty/hash status reported for:
/c/Users/justin/TaskCompassBackups/windows-baseline-20261114-140907/before-migration.sql.
The new migrator password entries then differed; runner stopped with
STOP: Passwords differ.

Reviewed current migration-setup branch and readNewPassword: password matching
finishes before applyTemplate migration is invoked. Therefore this attempt did
not execute CREATE USER/GRANT or migrations; it created only the private baseline
backup directory/files after read-only preflight. No migration account credential
was set by the mismatched entries. Actual backup hash was not included in the
transcript. This is a pre-provisioning input rejection, not partial SQL failure.

It is safe for the owner to repeat the already approved stage 05 with a newly
evaluated timestamp/new directory; preserve the prior backup and do not reuse its
existing directory or overwrite/delete it. The repeated stage rechecks the empty
schema and absence of new account names before any provisioning. Stop if those
checks differ. No additional repair SQL/privilege change is needed. Keep/save the
same intended new password in a password manager and enter it twice consistently.

The original Windows stages 05–07 approval remains limited to that concrete plan;
this recommendation does not authorize partial-SQL repair/reset or unexpected
account reuse. Stage 05 remains incomplete pending successful retry. Files changed:
this milestone only. git diff --check passed; application tests not rerun because
no code changed. No agent live SQL, backup, provisioning, migration, commit or push.

## Windows stage 05 provisioned; diagnostic-handler correction — 2026-11-14

The owner reran the approved stage 05 with a new baseline directory on feldria-w10
/ MariaDB 13.0.2. Owner transcript reports all preflight/diagnostic/PUBLIC checks
passed and a successful nonempty/checksummed baseline backup at:
/c/Users/justin/TaskCompassBackups/windows-baseline-20261114-141340/before-migration.sql.
After matching new-password prompts, the script reported five instances of:
`phase11-validate.sh: line 166: newPassword: unbound variable`.
It then reported exact direct grants/no explicit roles for taskcompass_migrate
and PASS: baseline backup and fresh migration account. Thus stage 05 provisioning
and grant validation succeeded per owner output; the native diagnostic handling
had a bug. Do not rerun stage 05 or recreate/reset this existing account.
No migration has been reported yet for Windows; stage 06 is next.

Root cause: applyTemplate deliberately unsets newPassword after provisioning.
Later clients emit MariaDB 13's password-option warning to stderr. The redactor
expanded the now-unset variable under set -u and exited in its process substitution,
which could suppress those diagnostics without failing successful client calls.
This is not evidence of credential leakage or grant failure; existing direct-grant
validation still returned PASS. Preserve the error record rather than treating
stage 05 as wholly clean.

Correction: scripts/phase11-validate.sh now uses default-empty expansion for all
three optional credential variables in the diagnostic handler. Secret clearing is
retained; no password/grant/SQL changes or extra dependency. Regression added to
tests/Phase11ValidationScripts.test.sh: fake native warnings on every client,
including post-provisioning calls after secret clearing; require the explanatory
TLS message, no raw credential fixture and no unbound-variable diagnostic.

Agent validation (Linux, mock-only):

```bash
bash -n scripts/phase11-validate.sh tests/Phase11ValidationScripts.test.sh
ctest --test-dir build --output-on-failure -R Phase11ValidationScripts
git diff --check
```

Results: syntax/whitespace passed; focused CTest 1/1 passed in 1.13 seconds.
No full C++ rebuild or broad CTest rerun because only a shell diagnostic path and
its mock regression changed; focused suite covers the complete scripted fake
workflow. Native Windows correction verification is pending next owner run.
Files changed: runner, helper test and this milestone. No agent live SQL, account
change, provisioning, backup, migration, restore, commit or push.

Next owner step after transferring the corrected runner/test files to Windows:

```bash
bash scripts/phase11/06-migrate.sh --approved --backup-dir "$baselineDirectory"
```

Use the same terminal's baselineDirectory, expected path
/c/Users/justin/TaskCompassBackups/windows-baseline-20261114-141340. Confirmation:
MIGRATE taskcompass_dev. Password order: administrator root, then the newly saved
migration account password. Existing stages 05–07 approval covers this exact
migration; stop on any error and preserve partial DDL, never reset/retry blindly.

## Owner-reported Windows stage 06 success — 2026-11-14

Owner executed the approved command:

```bash
scripts/phase11/06-migrate.sh --approved --backup-dir "$baselineDirectory"
```

Reported target: feldria-w10 / MariaDB 13.0.2 / 127.0.0.1:3306, UCRT64 on
MINGW64_NT-10.0-19045. Baseline before-migration.sql checksum: OK. Administrator
preflight and exact migration-account direct grants/no explicit roles passed;
PUBLIC had no write/DDL authority covering either TaskCompass schema.
TaskCompassMigrate apply and verification both reported:

```text
Schema 1 verified; dataset 457bb52b-fea4-4347-9c47-675c6b035c90
Schema 1 verified; dataset 457bb52b-fea4-4347-9c47-675c6b035c90
PASS: migration and verification.
```

This establishes owner-reported successful Windows development migration 001 and
schema verification. Windows dataset UUID is distinct from Linux's
21be9da7-b767-4578-b972-cc9a35122ddb, as expected for independently initialized
local databases; do not compare Windows restore against the Linux UUID. No
separate numeric process exit transcript was supplied; the final PASS follows
both successful CLI calls. No unbound-variable diagnostic appeared in this stage.

Next approved owner step: scripts/phase11/07-runtime-setup.sh --approved.
Confirmation: PROVISION taskcompass_app AND REVOKE migrator writes. Password order:
migration-account verification, administrator root, new runtime account password
twice. Stop on failure or unexpected existing account state; do not rerun stage
06, restore development, or reset schemas. Windows runtime provisioning/final
grants, interactive GUI/restart and populated restore remain pending.

Only this milestone document changed. git diff --check passed. No build/CTest
rerun for this documentation-only owner-result record. No agent live SQL,
provisioning, migration, backup, restore, commit or push.

## Owner-reported Windows stage 07 success with stale redactor symptom — 2026-11-14

Owner ran the approved scripts/phase11/07-runtime-setup.sh --approved, supplying
the complete confirmation. Migrator verification reported schema 1/dataset
457bb52b-fea4-4347-9c47-675c6b035c90. On feldria-w10 / MariaDB 13.0.2 / UCRT64,
preflight migration grants and PUBLIC checks passed. After runtime creation and
revocation, all three account grant checks passed:

```text
PASS: exact direct grants and no explicit roles for taskcompass_app.
PASS: exact direct grants and no explicit roles for taskcompass_migrate.
PASS: exact direct grants and no explicit roles for taskcompass_dev.
PASS: runtime grants and migrator write revocation.
```

The transcript again contains line 166 newPassword: unbound variable during native
stderr processing after secret clearing. This is consistent with the older Windows
runner still being used; the agent has not inspected that machine's file directly.
The repository's current loop uses default-empty credential expansions:

```bash
for secret in "${adminPassword:-}" "${accountPassword:-}" "${newPassword:-}"; do
```

Transfer that corrected scripts/phase11-validate.sh to Windows (or make this exact
loop correction in VIM); no C++ rebuild/database changes are required. Do not rerun
stage 07: its account creation and final grant validation succeeded per the owner
transcript. Keep the new runtime credential securely. Error-handler output problems
do not authorize password/grant resets or repeating provisioning.

Extended tests/Phase11ValidationScripts.test.sh so runtime provisioning/revocations,
not just migration-account provisioning, receive warnings on every mock client;
require successful final grants, explained TLS notice, no unbound-variable errors
and no credential fixture in output. Agent validation:

```bash
bash -n tests/Phase11ValidationScripts.test.sh
ctest --test-dir build --output-on-failure -R Phase11ValidationScripts
git diff --check
```

Focused suite passed 1/1 in 1.12 seconds. Existing runner fix confirmed by source
inspection; no additional production/script implementation change in this step.
No broad rebuild/CTest because only the regression test and milestone changed;
focused suite exercises the full mock workflow. Native corrected Windows redactor
behavior remains pending confirmation. No agent live SQL, account change,
provisioning, migration, backup, restore, commit or push.

Next: scripts/phase11/08-gui.sh after updating the Windows runner. Password order:
root for read-only final grant checks, then taskcompass_app runtime password.
Perform the GUI checklist and close/relaunch via the same stage for persistence.
Windows stages 08 onward are not yet validated; stage 10 restore retains its
separate exact-file approval. Phase 1.1 remains incomplete.
