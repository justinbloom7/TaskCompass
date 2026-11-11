# Implementation roadmap

Current work is **Phase 0.2: documentation, design and license**. The existing
Phase 0.1 prototype remains implemented but validation-incomplete. All production
phases below require explicit authorization; this plan does not start Phase 1.
The architecture stays C++20/Qt 6 Widgets/CMake with MariaDB, a shared UI-independent
core, later mobile SQLite and Linux/Windows bidirectional synchronization service.

| Phase | Intended delivery | Dependencies / principal exit gate |
| --- | --- | --- |
| 0 | Initial architecture/organization review | Recorded requirements and single-parent ownership decisions. |
| 0.1 | Desktop technology prototype | Existing implementation; finish isolated MariaDB, Windows and interactive checks. |
| 0.2 (current) | Requirements, interactions, command design, logical schema, roadmap and GPL license | Documentation consistency and licensing review; no production features. |
| 1.1 | Persistent task foundation with undo from its first edits | Close 0.1 gates, review 0.2 policies, explicit authorization; restart/transaction/history tests. |
| 1.2 | Organizational management | 1.1 command/storage core; legal ownership, nested navigation and undoable moves/archive. |
| 1.3 | Everyday progress, contexts, numeric priority and drag ordering | 1.1/1.2; context order first, then grouped insertion/atomic moves and undo. |
| 1.4 | Due/scheduled dates, presentation and basic filters | 1.3; shared calendar logic, minimal persisted day allocations; due/allocation separation. |
| 2 | Recurrence, dependencies, related/next actions | 1.x; stable occurrence identity and preserved instance history. |
| 3 | Quarter/sprint/week/day planning and period filters | 1.4/2; persisted period ranges and independent objective selections. |
| 3.1 | Custom task fields | 3 and shared commands/typed filtering; no Task column per custom field. |
| 4 | Guided planning | 3; resumable drafts and atomic, stale-aware confirmation. |
| 5 | Service and synchronization | Stable persistence/contracts; conflict/retry/tombstone/authentication validation. |
| 6 | Native mobile interface | 5 plus framework/licensing/device evaluation; separate possibly reduced UI. |

## Phase 0.1 actual validation — 2026-11-10

Implementation: QMainWindow with menus/toolbar/status bar, horizontal splitter,
fixture navigation tree, sortable/filterable task table, modal editing/new-task
flows, shortcuts, and QSettings layout restoration. A separate QCoreApplication
probe lists/loads QMYSQL and optionally connects to an explicitly configured
isolated database. It never creates tables/users/databases or accesses eEnvelopes.
The desktop prototype does not link SQL or connect to a server.

| Acceptance check | Observed result |
| --- | --- |
| Environment inspection before GUI implementation | GCC 11.5.0, CMake 3.31.8, Ninja 1.10.2 and MariaDB 10.5.29 packages present; Qt 6 development/QMYSQL and connector headers initially missing. Initial configure stopped at missing Qt6Config.cmake. |
| Dependencies | User confirmed installation of qt6-qtbase-devel 6.10.3, qt6-qtbase-mysql 6.10.3 and mariadb-connector-c-devel 3.2.6 after the sudo attempt could not authenticate. |
| C++20/Widgets build on CentOS Stream 9 | Passed: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`, then `cmake --build build --parallel 2`. |
| Model/dialog/action/layout tests | Passed: eight behavior cases in PrototypeTests, including proxy sorting without source reordering, consistent ownership/view fixtures, editing sorted selection, filtering/navigation, independent capture, cancel/validation and layout restoration. |
| Automated application startup | Passed: GuiStartup starts the real executable offscreen and closes with temporary settings. Both CTest targets pass via `ctest --test-dir build --output-on-failure`. |
| Actual display startup | Passed: `./build/TaskCompassPrototype --smoke-test` showed/closed on DISPLAY=:1 with exit 0. Interactive visual/keyboard usability review remains pending. |
| Qt SQL/MariaDB library compatibility | QMYSQL available and loadable; plugin resolves installed libmariadb.so.3 with no missing libraries. |
| Diagnostic with no connection | Passed: `./build/TaskCompassDbDiagnostic` lists drivers and skips connection with exit 0; automated incomplete-configuration case returns exit 3 without networking. |
| Successful isolated database connection/version query | Pending: no dedicated database connection configuration supplied; no server connection attempted. |
| Windows build/runtime/plugin deployment | Pending: no Windows environment available; instructions supplied, no Windows success claimed. |

**Phase 0.1 is not complete.** Finish the live isolated MariaDB check, Windows
build/runtime checks and interactive desktop review before closing the milestone.
The observed Linux results support Widgets as a suitable starting point; they do
not validate the full task manager, synchronization or mobile architecture.
See [Building.md](Building.md) for exact commands and limitations. No schema,
migration, persistent task data, sync or mobile implementation was introduced.

### Organizational refinement validation

The current hierarchy decisions supersede the earlier many-to-many organizational
link proposal. The in-memory tree now nests projects and objectives under their
owners, includes direct pillar objectives and project tasks, and shows independent
inbox work. Objective and sprint branches reference the same fixture keys/rows;
one objective is selected in two static sprint views. New samples are independent
captures rather than inheriting an arbitrary navigation filter as ownership.

Rebuilt with `cmake --build build --parallel 2` and reran
`ctest --test-dir build --output-on-failure`: both CTest targets passed. The added
fixture-view case verifies hierarchy contexts and one edited task row shared by
organizational/objective/sprint presentations. No production ownership model,
planning algorithms, database/schema or sync behavior was implemented. Phase 0.1
is ready to continue its outstanding validation; Phase 1 still requires approval.

## Acceptance, prerequisites and deferrals

### Phase 0.1 — finish the technology validation

Prerequisite: the existing prototype and approved development packages. Deliver
read-only SELECT VERSION() evidence against an existing isolated MariaDB database,
a Windows build/runtime/plugin-deployment check on Windows/VM, and an interactive
desktop usability review. Preserve the existing offscreen tests. Automated checks:
compile/CTest/diagnostic error paths; manual checks: real server/Windows/UI results.
Complete only when those pending checks have evidence. Defer all actual domain,
persistence, schema, sync and mobile features. Existing Linux results below are
historical evidence, not new claims that the remaining checks passed.

### Phase 0.2 — requirements and architecture refinement (current)

Prerequisite: repository/prototype review and owner decisions. Deliver updated
requirements/architecture/model, Interactions.md, Licensing.md, GPL LICENSE and
this phased plan. User-visible result is a reviewable design, not new GUI controls.
Automated validation: Markdown links/fences/consistency, unchanged prototype source
hashes, exact standard-license copy. Manual review: progress semantics, priority/
context policy, undo failure handling, date boundaries and license obligations.
Exit when requested designs are documented and remaining policy choices explicit;
owner approval is needed before their implementation. Defer all production code,
database changes, dependency installs, synchronization and mobile work.

### Phase 1.1 — persistent task foundation

Prerequisites: close 0.1 checks; approve persistence/command design and authorize
implementation. Deliver the smallest shared domain/application layer, dataset/task
storage, numbered MariaDB migrations, capture/edit, active/completed lifecycle,
completedAt, default active filtering, durable history and dormant change tracking.
WorkEntry may initially contain completion history only; actual progress recording
arrives in 1.3. Do not deploy the complete logical schema.

Introduce CommandService, receipts/inverses and commit-gated session undo/redo with
the first create/edit/complete/reopen commands. User can capture independently,
restart and recover edits; completed rows hide by default and can be shown/reopened.
Tests: real isolated MariaDB, migration recovery, invalid completion states,
transaction rollback, failed undo/redo without index drift, redo invalidation,
create-undo with dependencies, restart persistence, and GUI consistency across views.
Manual: capture/edit/undo during a session, restart persistence and explicit reopen,
backup/restore and clear errors. Session undo itself does not survive restart.
Exit when persistent edits/history stay atomic and these workflows pass. Defer
organization, contexts/order, progress, due/allocations, recurrence, periods and sync.

### Phase 1.2 — organizational management

Prerequisite: 1.1 command/migration layer. Deliver pillar/subpillar, project/
subproject and flat objective create/edit/rename/move/archive/restore controls,
parent-aware task creation/editing, hierarchy navigation and undoable changes.
User sees derived organizational context with no redundant ownership fields.
Tests: exact parent combinations, deep trees/cycles, cross-dataset rejection,
subtree moves, inherited archive, explicit restore flags, deletion-with-children
rejection, inverse failure and transaction rollback. Manual: menus/context menus,
Move dialogs/impact preview, alternate views and accessibility. Exit when IDs/history/
schedules survive reorganization and undo. Defer progress/context groups and drag,
recurrence, advanced planning and organizational destructive subtree deletion.

### Phase 1.3 — everyday task management

Prerequisite: 1.1/1.2; review priority/drop/cardinality/rank policies. First deliver
context CRUD/assignment and persistent context order, then numeric priorities and
rank storage, then grouped drag-and-drop. Also deliver Worked Today, Done for Today,
Resume Today, consistent completion/reopen and undo/redo for all these operations.
TaskDayDisposition allows progress/day intent without requiring a scheduled date.

User can record work without completing, return on later dates, undo work markers,
and see No context last. Drops have a between-row line and destination/priority
preview; cancelled/invalid drops do nothing, cross-context changes commit once.
Tests: midnight/date capture, multiple sessions/corrections, day closure vs progress,
completion/reopen invariants, context deletion/undo, archived context behavior,
context-before-priority sorting, equal-priority/head/tail/empty drops, key collisions/
local rebalance, persistence/restart and full drop rollback/undo. Manual: insertion
line visibility, autoscroll, empty/unassigned destinations, keyboard alternatives,
sort/filter disabling and post-drop position. Exit when ordering is deterministic,
persistent, atomic and undoable. Defer dated scheduling integration to 1.4, occurrence
behavior to 2, per-day manual-order overrides and multiple context assignment.

### Phase 1.4 — dates, presentation and filters

Prerequisite: 1.3 and approved calendar-zone/week-start policy. Deliver due dates,
explicit date scheduling/rescheduling with minimal daily PlanningPeriod/Allocation
rows, advisory daily workload, theme-aware due colors/text, basic status/date/
context/priority/organizational filters and other sorts. Minimal daily persistence
is deliberately introduced here; broad planning-period management waits for 3.
Integrate existing day-closure commands with existing daily allocations, never
create allocations just for recording progress.

Tests: inclusive editor/half-open storage, overdue/by-today/by-tomorrow/week end,
missing due date, leap days/DST/week starts, zone changes without history rewriting,
completion default filters, due-vs-scheduled distinction, explicit rescheduling with
history/undo and retained unfinished work. Manual: light/dark/high-contrast selection,
non-color labels, filter clarity and warnings without hard capacity restrictions.
Exit when date views use shared calculations and planned dates never masquerade as
due dates. Defer sprint/quarter filters until persisted periods in 3; saved-filter
editors and custom visual-rule builders remain later enhancements.

### Phase 2 — advanced task management

Prerequisite: 1.x command/storage/date foundations. Deliver recurring templates,
stable instances, repeat rules/history, occurrence completion/work/order overrides,
dependencies, next actions and related tasks. Context stays template-level initially;
instance context gestures explicitly disclose series-wide effects.
Tests: idempotent occurrence generation, missed work retained, month-end/DST,
reschedule identity, instance/template separation, rule edits, dependency cycles,
recurring blocking-link policy and undo. Manual: combined daily categories, backlog,
instance vs series scope and history correction. Exit when recurring work preserves
history without auto-rollover and next-action results match rules. Defer full RRULE,
completion-relative recurrence unless approved, per-occurrence custom fields and sync.

### Phase 3 — multi-horizon planning

Prerequisite: 1.4 dates/allocations and 2 task semantics. Deliver persisted calendar
quarters, calendar-month/custom-date sprints, weekly/daily periods, explicit overlap/
current-period resolution, objective selections/period rank, due/scheduled sprint/
quarter filters, unfinished reviews, advisory capacity and protected-day preferences.
User can select one objective across sprints without moving/completing it; protected
days remain overridable, and period edits do not move dated work silently.
Tests: ranges/boundaries, current-period gaps/overlaps, range edits/undo, UUID/link
stability, objective deselection with allocations intact, filter predicates and no
double-counted workload. Manual: review/reschedule, objective priorities and warnings.
Exit when persisted periods drive filters and planning selection is distinct from
ownership/due/scheduling/history. Defer fiscal-quarter generation and wizard UI.

### Phase 3.1 — custom fields

Prerequisite: 3 and typed filters/commands/migrations. Deliver task definitions,
typed values/options, dialog controls, optional columns, sorting/filtering and
undoable definition/value edits. User adds a field without altering Task schema.
Tests: all types, null vs false/zero, decimal precision, choice ownership/cardinality,
option archive/rename, incompatible definition edits, migration/restart and undo.
Manual: capture with missing required values, editors/columns and filter/sort clarity.
Exit when definitions and typed values persist safely with deterministic queries.
Defer project/objective value tables, formulas, scripts and occurrence custom values.

### Phase 4 — guided planning

Prerequisite: 3 (3.1 if custom fields appear in wizard screens). Deliver resumable
quarter/sprint/week/day planning sessions: review objectives/commitments, capture/
unallocated work, unfinished work, reschedule, select/allocate, review workload,
confirm. Tests: draft version/restart, no mutation on abandonment, stale inputs,
atomic confirmation/undo and no rollover. Manual: complete every wizard, resume
and cancel with advisory overrides. Exit when reviewed intent applies once safely.
Defer remote collaborative drafts and automatic scheduling algorithms.

### Phase 5 — synchronization

Prerequisite: stable migrations/UUIDs/outbox/command audit and domain conformance
fixtures; design endpoint ownership, authentication and retention. Deliver separate
Linux/Windows background service, MariaDB hub, authenticated HTTPS, enrollment/
bootstrap, bidirectional change exchange, conflicts, retries/tombstones and device
recovery. Financial data/models remain independent from eEnvelopes.
Tests: two simulated offline clients, lost responses/duplicates/crashes, revision
conflicts, delete/edit, ordering anchors, recurrence duplicates, expired cursors,
auth failures, inverse commands after remote changes and restored-device identity.
Manual: service without GUI, both host OSs, reconnect/conflict review and backup.
Exit when accepted changes converge without silent loss and session undo cannot
replay stale external state. Defer multi-hub replication and native mobile UI.

### Phase 6 — native mobile application

Prerequisite: 5, device access/macOS-Xcode, and a resolved licensing/distribution
route. Evaluate Qt Quick/QML vs other native presentation with shared C++ services;
choose a possibly reduced feature set. Deliver local SQLite/offline commands,
sync and selected capture/today/progress/completion/basic-editing flows; provider-
neutral voice adapter when scoped. Tests: adapter conformance, suspension/restart,
zone changes, offline mutations/conflicts and authentication recovery. Manual:
iPhone signing/device accessibility and chosen-channel compliance; Android only if
authorized. Exit when selected features work offline and converge safely on devices.
Desktop parity and mobile drag-and-drop are not initial requirements; architecture
does not forbid later mobile reorder gestures using the same commands.

## Recommended next step

Review Phase 0.2 policy recommendations and finish the three outstanding Phase 0.1
validation checks. Then explicitly authorize a small Phase 1.1 persistence/command
slice. Do not begin Phase 1 or deploy the whole proposed schema from this document.
