<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Phase 1.1 — Persistent Task Foundation

Status: Step A inspection/design delivered; awaiting owner approval for Step B.
Recorded: 2026-11-12. This document saves the proposed implementation plan;
it does not authorize account provisioning, migrations or production implementation.

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
The current code still uses disposable fixtures and implements no persistent tasks.

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

## Approval and execution record

- Step A: plan presented and saved; owner approval for Step B pending.
- Step B: not started. No production source, migrations or accounts created.
- Step C: not started. No Phase 1.1 build/integration validation claimed.
- Step D: append implementation results, limitations and remaining work here.

Approval items: minimal schema/privileges, standard-C++ shared types, separate
migration executable, commit-gated history controller and session-only undo.
Do not commit, merge, tag or push without explicit instruction.
