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
| Organizational management | Create/edit/rename/move/archive/restore pillars and nested subpillars; projects/subprojects; flat objectives. Create tasks under objectives, directly under projects, or independently. Validate cycles and parent constraints. |
| Daily use | Combined task categories; separate Worked Today and Completed quick actions; Done for Today/Resume as distinct daily-intent actions; user-defined contexts, numeric priority, drag insertion lines, moves and alternate views. |
| Progress/completion | Work against current local calendar date without completing; undo with history preserved; additional work on later days. Completion updates lifecycle/completedAt, hides by default, supports show filters, undo and explicit reopen; recurring operations target instances. No unrelated Task booleans. |
| Undo/redo | Shared typed commands, transactional inverse/redo receipts and persistent history from Phase 1.1. Desktop session history across views; failures do not move history index; sync compensation is a new revisioned operation. |
| Contexts | Entirely user-defined, optional; create/rename/archive/delete/assign/remove/reorder/group/filter. Persistent user-defined context sort positions; one optional primary TaskContext assignment initially, extensible to secondary contexts later. Context groups precede the final No context group regardless of task priority. |
| Numeric priority | Positive integers, 1 highest; every capture defaults to 5; explicitly editable; no arbitrary product maximum. Independent of context order, objective sprint rank, due dates and lifecycle. |
| Hidden manual order | Approved default daily order: context position, ascending numeric priority, hidden manual-order token within the context/priority group, stable task UUID. No visible subpriority field. |
| Drag/drop | Persistent within/between-context insertion with visible between-row line and destination context, no mutation on cancel, atomic assignment/order/priority change with undo. Within-priority drops change hidden order only; cross-priority drops adopt insertion-anchor priority shown in preview. Unrelated priorities and relative order remain unchanged. No context drop removes assignment. |
| Dates/presentation | Configurable theme-aware due-date text rules for overdue/today/tomorrow/later/completed with non-color labels and explicit precedence; due and planned dates are different. |
| Filters | Overdue, today/by today, tomorrow/by tomorrow, end-of-week, sprint/quarter due ranges, active/completed, no due date, context, numeric priority and organizational scope. Calendar-zone/week-start aware; completed hidden by default. |
| Periods | UUID/label/horizon/start/end for quarters and sprints; half-open stored bounds, calendar quarters/month sprints by default, custom sprint dates; disambiguate overlapping current periods. Objective selection never schedules tasks automatically. |
| Custom fields | Future task-first typed definitions/options/values for text/integer/decimal/date/boolean/single/multichoice; optional editors/columns/sort/filter; no physical Task column per user field. |
| License | GPL-3.0-or-later for project-owned work; dependency notices and release obligations retained; future iOS distribution needs specific review. |
| Separation | Lifecycle, allocation, horizon, period priority, actual work, and completion are distinct. Unallocated is not inherently a lifecycle state. |
| Capture | Fast inbox entry; future mobile voice capture independent of speech provider. |
| Long term | Someday/long-term work stays available without cluttering daily planning. |
| Current iteration | Phase 0.1 connectivity validation plus approved-ordering documentation after Phase 0.2. Dedicated empty development database and SELECT-only account permitted; no application tables, migrations, persistent task/ordering implementation, sync, mobile work or Git commit. |

## Proposed behavior needing review

| Decision | Alternatives | Recommendation and reason |
| --- | --- | --- |
| Desktop toolchain versions | Compatible Qt 6/compiler/CMake and connector versions | C++20/Qt 6 Widgets is decided; validate exact versions and MariaDB driver on CentOS and Windows. See Architecture.md. |
| Mobile strategy | Qt Quick/QML or another native-compatible presentation | Separate, potentially reduced mobile UI using shared C++ use cases; Qt Quick/QML remains provisional. Candidate features: capture, today, completion, Done for Today and basic task editing; scope undecided. Evaluate native iOS suitability, licensing, macOS/Xcode tooling and core integration when mobile work begins. Android optional. |
| Lifecycle vocabulary | Simple active/completed; add hold/cancel/archive | Propose active, onHold, completed, cancelled; archive separately. Final labels and reopen semantics need review. |
| Progress/day closure | Direct booleans; work-history-derived progress plus daily disposition | Recommend derived Worked Today, distinct daily closure, atomic lifecycle commands; no auto-allocation just to record work. Interactions.md specifies undo/reopen semantics. |
| Recurrence | Reopen same task; independent occurrences; cloned tasks | Template plus stable occurrences with independent completion and allocation; preserve history. Start with fixed calendar dates; completion-relative repetition later. |
| Missed recurring work | Accumulate; skip; coalesce | Retain generated unfinished occurrences; never silently move/complete them. Review UI needs a backlog summary. Decide generation window and optional explicit skip policy. |
| Scheduling granularity | Dates only; timed appointments | Date and period allocations first; distinguish due date from planned date. Time-of-day sessions deferred. |
| Current sprint/quarter | Reject overlap; allow overlap with explicit selection | Recommend permit overlapping custom periods, require explicit current choice when ambiguous, and show bounds. Defaults are calendar month/quarter. Range edits keep IDs and flag commitments for review without moving dated work. |
| Period relationships | Strict quarter→sprint→week→day tree; independent intervals | Independent date intervals because weeks/sprints cross quarter boundaries. Associations derived by overlap; no forced parent. |
| Session undo | QUndoStack as domain engine; receipt adapter; custom controller | Recommend shared CommandService + commit-gated desktop history adapter; evaluate QUndoStack bookkeeping only. Session stack default 100 commands, durable audit retained separately. |
| Archive/move with children | Mutate every descendant; inherited effective archive | Recommend parent-reference moves and effective descendant archive; preserve descendant explicit flags/history. Restore never silently unarchives ancestors/children. |
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
2. Implement the confirmed exclusive parent rules in DataModel.md when Phase 1.2
   is authorized; review archive/move policy and physical constraints first.
   Flat objectives and single-parent membership are settled, not open alternatives.
3. Approve the proposed CommandService/desktop history failure policy before
   Phase 1.1; introduce it with the first persistent edits, not retroactively.
4. Context/priority/hidden-order behavior is approved. Specify/test the canonical
   fractional-token format and bounded normalization before Phase 1.3. Calendar-zone
   behavior and remaining interaction recommendations still need review.
5. Resolve MariaDB physical naming/collation and migration portability before
   persistence implementation. Desktop/service MariaDB, mobile SQLite, and Windows
   builds on Windows remain the storage/platform direction. Mobile UI scope, toolkit
   and macOS/Xcode build access are later decisions, not desktop-prototype gates.

Later review gates: recurrence rules (month-end, DST, missed occurrences, edits to a
series); canonical rank encoding/rebalance; hard versus advisory dependency
behavior; optional Android; hosting/authentication; privacy/encryption; deletion
retention and maximum offline interval. None is silently treated as finalized.

## Resolved relationships and remaining choices

The prior many-to-many project/objective, task/objective and explicit pillar links
were proposals, not requirements that demand retention. They are replaced by the
confirmed single-parent hierarchy. No remaining confirmed requirement conflicts
with these cardinalities; context is inherited through organizational ownership.
Direct task-to-pillar ownership is excluded by the specified task parent choices.

Physical constraint/index choices and historical path snapshots remain open. The
recommended parent move/archive/restore UX is now detailed in Interactions.md;
retain current-context queries and reject deletion with live children until reassignment.
These details do not block Phase 0.1 fixture/technology validation. Organizational
membership is settled; sprint selection never becomes ownership or task scheduling.

## Phase 0.2 specification references

[Interactions.md](Interactions.md) defines menus/editors, daily state transitions,
context ordering, drop priority changes/feedback, precise date predicates and period
selection. [Architecture.md](Architecture.md) specifies commit-gated undo/redo and
shared boundaries. [DataModel.md](DataModel.md) proposes persistence additions;
[Roadmap.md](Roadmap.md) orders their delivery and validation. These documents
distinguish confirmed capabilities from recommended policies awaiting review.
[Licensing.md](Licensing.md) records the license grant, dependency evidence and
unresolved native-iOS release concerns. No Phase 0.2 behavior is implied to exist
in the Phase 0.1 prototype.

## Approved ordering behavior — 2026-11-11

Context position takes precedence over numeric priority; No context is always last.
New tasks default to 5; 1 is highest. Manual positions are hidden and persistent
within context/priority bands. Same-priority drags never change numeric priority;
cross-priority drags adopt the existing successor/predecessor anchor policy with
an explicit resulting-priority preview. Cross-context moves update assignment and
destination order together, including removal of assignment for No context, as one
undoable transaction. Never renumber unrelated priorities or reorder unrelated work.

Variable-length fractional tokens remain the recommended physical rank family,
with binary deterministic comparison. Token alphabet/encoding, length threshold,
normalization window/algorithm, and physical indexes remain implementation choices;
the user-visible behavior is settled. See Interactions.md and DataModel.md. This
approval does not authorize implementing ordering or starting Phase 1.1.
