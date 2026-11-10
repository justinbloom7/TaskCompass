# Requirements and review decisions

**Confirmed** means requested by the user. **Proposed** means a recommendation for
review, not a settled product or technology decision. Deferred features still
influence the design; they need not ship in the first usable release.

## Confirmed scope

| Area | Requirement |
| --- | --- |
| Product | Traditional desktop task manager with sophisticated guided planning; desktop is primary and eventually provides the complete feature set. Not a general life-management suite. |
| Desktop UI | C++20 with Qt 6 Widgets; CMake CLI/VIM workflow. Menus, toolbars, shortcuts, pillar/project trees, sortable/filterable task tables/lists and manual ordering, splitters/resizable panels, editors/dialogs, and guided planning. |
| Shared core | Shared C++ domain rules and application use cases independent of Widgets and QML; separate presentations, persistence, and future sync infrastructure. No generic UI abstraction framework. |
| Platforms | Offline desktop first on CentOS Stream 9; Windows executable builds on Windows/VM are acceptable; native iPhone app later, possible Android. |
| Storage/deployment | MariaDB on Linux, like eEnvelopes; local database operation with a path to self-hosted or managed online storage. Mobile must still work offline. |
| Internet service and sync | Native phone connects over the internet to a TaskCompass service hosted on Linux or Windows. Phone-local and service-side databases synchronize changes in both directions, including offline edits after reconnection. |
| Pillars | Editable/reorganizable, relatively permanent responsibilities; arbitrarily nested subpillars; directly contain projects and objectives. Task context is derived through its objective or direct project, not a task-to-pillar ownership link. |
| Objectives | Flat outcomes/milestones, lasting weeks to years; exactly one pillar/subpillar or project parent; may contain tasks and span multiple sprints. Lifecycle and parent are independent of sprint selection. |
| Projects | Bodies of related work, possibly long-running; exactly one pillar/subpillar or parent-project owner; may contain subprojects, objectives and direct tasks without objectives. |
| Tasks | One-time, continuing, recurring work; zero or one objective, otherwise zero or one direct project, or independent. Objective tasks inherit context; no redundant pillar/project ownership. No parent is required for capture. Dependencies, next actions and Done for Today behavior preserved. |
| Sprint objectives | Objectives and sprints are many-to-many selections. Removing a selection never deletes the objective/tasks or changes its parent. Selection, task scheduling and actual work/completion are separate; no automatic rollover. |
| Navigation views | Project-oriented pillar/subpillar/project/objective tree with direct-project and objective tasks; objective-oriented view with ownership context; sprint-oriented view of selected objectives/tasks. All reference the same records. |
| Planning | Quarterly, sprint, weekly, daily; configurable sprint duration initially one month; eventual wizard sessions. |
| Reviews | Objectives, commitments, unallocated tasks, unfinished scheduled work, rescheduling, period selection, workload, and priorities. |
| Unfinished work | Remains scheduled until explicitly rescheduled; review in planning wizards. No automatic rollover. |
| Capacity | Advisory warnings if implemented; never a hard scheduling restriction. |
| Protected days | Configurable preferences, including Sunday family/council/planning emphasis; manual scheduling always possible. |
| Daily use | Combined task categories; priorities and manual drag ordering; contexts; completion, Done for Today, moves; pillar/project/flat views. |
| Separation | Lifecycle, allocation, horizon, period priority, actual work, and completion are distinct. Unallocated is not inherently a lifecycle state. |
| Capture | Fast inbox entry; future mobile voice capture independent of speech provider. |
| Long term | Someday/long-term work stays available without cluttering daily planning. |
| Current iteration | Organizational documentation refinement and limited Phase 0.1 fixture updates only: C++20/Qt 6 Widgets with disposable fixtures, optional isolated QMYSQL diagnostic and focused tests. No actual domain features, task persistence, schema/migrations, sync, mobile code or Git commit. Dependency installation requires approval. |

## Proposed behavior needing review

| Decision | Alternatives | Recommendation and reason |
| --- | --- | --- |
| Desktop toolchain versions | Compatible Qt 6/compiler/CMake and connector versions | C++20/Qt 6 Widgets is decided; validate exact versions and MariaDB driver on CentOS and Windows. See Architecture.md. |
| Mobile strategy | Qt Quick/QML or another native-compatible presentation | Separate, potentially reduced mobile UI using shared C++ use cases; Qt Quick/QML remains provisional. Candidate features: capture, today, completion, Done for Today and basic task editing; scope undecided. Evaluate native iOS suitability, licensing, macOS/Xcode tooling and core integration when mobile work begins. Android optional. |
| Lifecycle vocabulary | Simple active/completed; add hold/cancel/archive | Propose active, onHold, completed, cancelled; archive separately. Final labels and reopen semantics need review. |
| Continuing work | Daily flag; append-only work records | Record work entries plus allocation outcomes; allow multiple sessions/day and a deliberate reopen after Done for Today. Duration optional. |
| Recurrence | Reopen same task; independent occurrences; cloned tasks | Template plus stable occurrences with independent completion and allocation; preserve history. Start with fixed calendar dates; completion-relative repetition later. |
| Missed recurring work | Accumulate; skip; coalesce | Retain generated unfinished occurrences; never silently move/complete them. Review UI needs a backlog summary. Decide generation window and optional explicit skip policy. |
| Scheduling granularity | Dates only; timed appointments | Date and period allocations first; distinguish due date from planned date. Time-of-day sessions deferred. |
| Sprint month | Fixed 30 days; calendar month; anchored month interval | Default calendar months. Configurable interval/unit and anchor later; persist actual period bounds so preference changes do not rewrite history. |
| Period relationships | Strict quarter→sprint→week→day tree; independent intervals | Independent date intervals because weeks/sprints cross quarter boundaries. Associations derived by overlap; no forced parent. |
| Priority/order | Global score; period rank; manual order | Period-scoped optional priority plus separate manual ordering. No fixed score formula or mandatory bands yet. |
| Inbox/someday | Lifecycle statuses; organizational attributes | Inbox as triage attribute; someday as planning visibility. Both independent of active/completed status and allocation. |
| Dependencies | Hard blocking; warning; general relation graph | Completion dependencies block suggested “next actions”; explicit manual scheduling still allowed with a warning. Non-blocking related links separate. |
| Wizard persistence | Ephemeral pages; saved draft | Saved resumable draft; explicit confirmation applies changes atomically, and revalidates changed tasks. |
| Hosting | Managed provider; user-operated endpoint; local network | MariaDB default; configurable HTTPS endpoint with self-hosted/managed server storage. Alternative service adapters remain possible; no sync service now. |
| Conflicts | Last writer wins; CRDT; explicit conflicts | Optimistic revisions with explicit review for competing edits; avoid silent data loss and premature CRDT complexity. |

## Decisions to settle before the first application slice

1. Select compatible C++20/Qt 6 Widgets, CMake, compiler and MariaDB connector
   versions and a distribution approach; the desktop language/framework is decided.
   Decide whether shared value types use only standard C++ or allow Qt Core, while
   keeping Widgets and QML out of domain/application interfaces.
2. Implement the confirmed exclusive parent rules in DataModel.md when Phase 1
   is authorized; first resolve deletion/reparent/archive UX and physical constraints.
   Flat objectives and single-parent membership are settled, not open alternatives.
3. Confirm minimal lifecycle, inbox/someday meaning, date scheduling, and Done for
   Today/reopen semantics. A completed scheduled session must not finish continuing work.
4. Confirm calendar-month sprint default and local-date/time-zone behavior.
5. Resolve MariaDB physical naming/collation and migration portability before
   persistence implementation. Desktop/service MariaDB, mobile SQLite, and Windows
   builds on Windows remain the storage/platform direction. Mobile UI scope, toolkit
   and macOS/Xcode build access are later decisions, not desktop-prototype gates.

Later review gates: recurrence rules (month-end, DST, missed occurrences, edits to a
series); priority versus drag-order precedence; hard versus advisory dependency
behavior; optional Android; hosting/authentication; privacy/encryption; deletion
retention and maximum offline interval. None is silently treated as finalized.

## Resolved relationships and remaining choices

The prior many-to-many project/objective, task/objective and explicit pillar links
were proposals, not requirements that demand retention. They are replaced by the
confirmed single-parent hierarchy. No remaining confirmed requirement conflicts
with these cardinalities; context is inherited through organizational ownership.
Direct task-to-pillar ownership is excluded by the specified task parent choices.

Parent move/delete/archive/restore UX, physical constraint/index choices, and any
historical path snapshots remain open. Recommend retaining current-context queries
and prohibiting deletion of parents with live children until explicit reassignment.
These details do not block Phase 0.1 fixture/technology validation. Organizational
membership is settled; sprint selection never becomes ownership or task scheduling.
