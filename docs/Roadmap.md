# Implementation roadmap

Every phase adds a usable capability or validates a concrete risk. C++20/Qt 6
Widgets is the desktop decision; the logical schema and other open choices remain
proposals until reviewed. This roadmap authorizes no
feature implementation during the current documentation phase.

| Phase | Deliverable | Acceptance / validation gate |
| --- | --- | --- |
| 0 — Review (current) | Requirements, stack comparison, logical schema, checkbook reuse findings, open decisions. | Review decisions listed in Requirements.md; agree on first slice and style. No feature code or dependencies. |
| 0.1 — Desktop technology prototype (next, only when instructed) | Minimal C++20/Qt 6 Widgets application built with CMake CLI on CentOS Stream 9 and Windows/VM; hierarchical tree, task table/list and task-editing dialog, with menus/toolbars/shortcuts and splitter layout. | Temporary in-memory fixtures only; demonstrate driver availability and read-only connectivity to an isolated MariaDB instance. Document exact versions, build/run steps and packaging dependencies. No task features, schema/migrations, sync, or mobile UI. |
| 1 — Small offline desktop | Capture into inbox; organize into nested pillars/projects, flat objectives and contexts; active/completed tasks, someday filter; one-time and continuing tasks; schedule a date, retain unfinished dated work, reschedule, manually order today, complete, Done for Today. | User can capture, restart offline, organize, plan today, record continuing work without completing it, finish a task, and explicitly move unfinished work. No online login or remote server required; local MariaDB must run. Backup/restore works. Windows packaging is checked early. |
| 2 — Reliable everyday use | Calendar recurrence and instance history, dependency-based next actions, related links, estimates, useful pillar/project/flat filters, keyboard capture and drag ordering. | All categories share daily view; missed instances stay visible; duplicates are prevented; cycles rejected; core date/recurrence and reopen behavior validated. |
| 3 — Multi-horizon planning | Concrete quarter/sprint/week/day periods; reusable period objective priorities; unallocated and unfinished reviews; workload warnings; protected-day preferences. | Month-default sprints are configurable; overlapping horizons do not double-count daily load; period end changes no objective lifecycle or schedule automatically. |
| 4 — Guided planning | Resumable wizards for each horizon: review goals/commitments, inbox/unallocated, unfinished work, reschedule, select/allocate, review workload/priorities, confirm. | Abandoned drafts do not alter authoritative schedules; explicit confirmation is atomic and detects stale inputs; scheduling on protected days remains possible. |
| 5 — Sync contract and service | Protocol/reuse review with checkbook; Linux/Windows background service with authenticated internet HTTPS API, MariaDB persistence, enrollment/bootstrap, incremental push/pull, tombstones, conflict review, backup and restore policy. | Service runs on Linux and Windows with GUI closed; desktop and phone changes converge in both directions across disconnections without silent data loss; retries create no duplicates; revisions, authorization, cursor expiry and conflicts tested. Independent application data preserved. |
| 6 — Offline native iPhone | Evaluate Qt Quick/QML and alternatives, licensing, native iOS tooling and C++ core reuse; separate UI and finalized mobile scope, potentially reduced. Candidate features: capture, today, completion, Done for Today and basic editing; local SQLite, shared protocol; provider-neutral voice capture when scoped. Android optional. | Selected mobile workflows operate offline and synchronize safely; suspend/resume, time-zone changes, duplicate recurrence generation, and authentication recovery tested on actual devices. |

## Keep phase one small

Implement in vertical slices rather than creating the entire proposed schema at once:

1. Complete the separate phase 0.1 Widgets technical prototype only after explicit
   instruction. Desktop does not wait for a Qt Quick/mobile prototype. Validate
   C++20, CMake CLI, the MariaDB SQL plugin and conventional desktop controls on
   CentOS Stream 9, then build on Windows using a Windows machine or VM. Keep
   temporary UI fixtures distinct from TaskCompass feature implementation.
2. Add a minimal domain/application/storage boundary, numbered migrations, dataset
   identity, task capture/list/restart persistence, and transactionally recorded local
   changes. Use an isolated MariaDB test database, never the checkbook database.
   No remote server or synchronization service is required.
3. Add organization/link tables and queries as their UI is introduced. Keep objective
   hierarchy deferred; add no generalized graph framework.
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
