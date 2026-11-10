# Implementation roadmap

Every phase adds a usable capability or validates a concrete risk. C++20/Qt 6
Widgets is the desktop decision; the logical schema and other open choices remain
proposals until reviewed. The user authorized Phase 0.1 technical prototype
implementation; actual product/domain features remain outside this iteration.

| Phase | Deliverable | Acceptance / validation gate |
| --- | --- | --- |
| 0 — Initial review | Requirements, stack comparison, logical schema, checkbook reuse findings, open decisions. | Review decisions listed in Requirements.md; agree on first slice and style. No feature code or dependencies. |
| 0.1 — Desktop technology prototype (implemented; validation incomplete) | Minimal C++20/Qt 6 Widgets application built with CMake CLI on CentOS Stream 9 and Windows/VM; hierarchical tree demonstrating pillar/project ownership, objective/direct-project/independent task fixtures, task table/list and task-editing dialog, with menus/toolbars/shortcuts and splitter layout. | Temporary in-memory fixtures only; demonstrate driver availability and read-only connectivity to an isolated MariaDB instance. Document exact versions, build/run steps and packaging dependencies. No task features, schema/migrations, sync, or mobile UI. |
| 1 — Small offline desktop | Capture into inbox; organize into nested pillars/projects, flat single-parent objectives and contexts; objective/direct-project/independent task ownership; active/completed tasks, someday filter; one-time and continuing tasks; schedule a date, retain unfinished dated work, reschedule, manually order today, complete, Done for Today. | User can capture, restart offline, organize, plan today, record continuing work without completing it, finish a task, and explicitly move unfinished work. No online login or remote server required; local MariaDB must run. Backup/restore works. Windows packaging is checked early. |
| 2 — Reliable everyday use | Calendar recurrence and instance history, dependency-based next actions, related links, estimates, useful pillar/project/flat filters, keyboard capture and drag ordering. | All categories share daily view; missed instances stay visible; duplicates are prevented; cycles rejected; core date/recurrence and reopen behavior validated. |
| 3 — Multi-horizon planning | Concrete quarter/sprint/week/day periods; reusable period objective priorities; unallocated and unfinished reviews; workload warnings; protected-day preferences. | Month-default sprints are configurable; overlapping horizons do not double-count daily load; period end changes no objective lifecycle or schedule automatically. |
| 4 — Guided planning | Resumable wizards for each horizon: review goals/commitments, inbox/unallocated, unfinished work, reschedule, select/allocate, review workload/priorities, confirm. | Abandoned drafts do not alter authoritative schedules; explicit confirmation is atomic and detects stale inputs; scheduling on protected days remains possible. |
| 5 — Sync contract and service | Protocol/reuse review with checkbook; Linux/Windows background service with authenticated internet HTTPS API, MariaDB persistence, enrollment/bootstrap, incremental push/pull, tombstones, conflict review, backup and restore policy. | Service runs on Linux and Windows with GUI closed; desktop and phone changes converge in both directions across disconnections without silent data loss; retries create no duplicates; revisions, authorization, cursor expiry and conflicts tested. Independent application data preserved. |
| 6 — Offline native iPhone | Evaluate Qt Quick/QML and alternatives, licensing, native iOS tooling and C++ core reuse; separate UI and finalized mobile scope, potentially reduced. Candidate features: capture, today, completion, Done for Today and basic editing; local SQLite, shared protocol; provider-neutral voice capture when scoped. Android optional. | Selected mobile workflows operate offline and synchronize safely; suspend/resume, time-zone changes, duplicate recurrence generation, and authentication recovery tested on actual devices. |

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

## Keep phase one small

Implement in vertical slices rather than creating the entire proposed schema at once:

1. Finish pending validation of the authorized phase 0.1 Widgets prototype before
   starting product features. Desktop does not wait for a Qt Quick/mobile prototype. Validate
   C++20, CMake CLI, the MariaDB SQL plugin and conventional desktop controls on
   CentOS Stream 9, then build on Windows using a Windows machine or VM. Keep
   temporary UI fixtures distinct from TaskCompass feature implementation.
2. Add a minimal domain/application/storage boundary, numbered migrations, dataset
   identity, task capture/list/restart persistence, and transactionally recorded local
   changes. Use an isolated MariaDB test database, never the checkbook database.
   No remote server or synchronization service is required.
3. Add single-parent organization references and constraints as their UI is
   introduced; objective hierarchy is explicitly excluded. PeriodObjective remains
   a separate selection table. Build project/objective/sprint views from the same
   records; add no generalized graph framework.
4. Add daily PlanningPeriod, Allocation, WorkEntry and the continuing-task operations.
   Broader planning periods and recurrence tables wait for their phases. Basic
   dependency work may wait until phase two without distorting task lifecycle.
5. Add backup/restore, error handling, and a local installation/run guide. Use the
   application for real tasks before expanding planning features.

Minimal first capture slice needs only dataset/task storage, migration metadata,
and dormant change tracking. Organization, daily scheduling and work history follow
within phase one, not a giant up-front schema deployment. LocalChange is not a sync
service: it records local mutations; network protocol/server tables remain deferred.

Mobile/Qt Quick validation belongs to phase six, including licensing, macOS/Xcode
build/signing, device lifecycle behavior and access to the shared C++ core through
mobile adapters. Share domain/application libraries now without creating mobile
projects or requiring Widgets/QML dependencies in those libraries.

## Validation approach

Test domain invariants rather than widget implementation details. Prioritize task
completion versus Done for Today, multi-day allocations, retained unfinished work,
rescheduling rollback, tree/dependency cycles, recurrence identity, month boundaries,
time-zone/DST calendar behavior, migration failure recovery, and backup restoration. Add
storage integration tests against an isolated MariaDB database, adding temporary
SQLite mobile databases when that adapter is built, and a few GUI smoke
checks for the actual workflows. Inject clock/IDs for deterministic examples.
Do not implement speculative mock frameworks or attempt every future test now.

Sync phase requires failure-injection tests (lost response after server commit,
local crash, concurrent updates/delete, reordering, stale cursor, revoked identity,
restored devices) and server transaction integration tests. Reuse checkbook failure
scenarios without importing its financial model. Mobile voice tests verify that
recognized text can be reviewed/corrected and does not couple task data to a vendor.

## Explicit deferrals

No synchronization endpoint, cloud provisioning, native mobile project, speech
provider, general workflow engine, CRDT framework, plugin system, attachments,
calendar-service integration, or analytics platform in the initial desktop release.
Revisit additional dependencies only with a concrete feature and maintenance benefit.
