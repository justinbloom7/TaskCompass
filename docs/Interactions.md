# Desktop interaction design — Phase 0.2

Requirements explicitly requested by the owner are confirmed. Policies labeled
**recommendation** are reviewable design choices. None of the controls described
here is implemented by this design phase. The Phase 0.1 sample status combo, text
priorities, fixture tags and table sorting do not implement these rules.

## Organization, menus and editors

Retain project-, objective- and sprint-oriented presentations of the same records.
Organizational context (pillar/project ancestry) is different from a user-defined
Context such as a work location or time block. Contexts never own tasks.

| Surface | Intended actions |
| --- | --- |
| File | New Task/quick capture, database selection/configuration later, Exit. No requirement to organize before capture. |
| Edit | Undo/Redo with descriptive labels (Ctrl+Z/Ctrl+Shift+Z), task editing, context management. Persistent commands commit immediately; no global Save required. |
| Organize menu / navigation context menu | New Pillar/Subpillar/Project/Subproject/Objective where legal; Edit/Rename; Move; Archive; Restore; Show Archived. Disable illegal parent types. |
| Task menu / toolbar / row context menu | New Task, Edit, Worked Today, Done for Today/Resume Today, Complete/Reopen, Set Context, Set Numeric Priority, Schedule/Reschedule. Selected-objective/project creation supplies that owner; global quick capture stays independent. |
| View / filter bar | Group by Context, context/priority/date/status/owner filters, sort mode, Show Completed, Show Archived, clear filters. Controls expose the active mode and date reference. |
| Planning | Select objectives for a period and inspect allocations; wizard workflows arrive in Phase 4. |
| Preferences / Help | Calendar zone/week start, context ordering, configurable visual rules; About/license notices. |

Use modal editors for title/description and relevant parent/lifecycle fields; a
Move dialog presents a legal destination tree with a full path. Preserve existing
UUIDs when renaming or moving. Objective editors offer pillar **or** project,
never another objective. Task editors choose independent, objective, or direct
project ownership; changing that choice clears incompatible parent references in
one command. Inline renames and priority edits use the same application commands.

**Recommended move/archive policy:** validate same dataset, parent availability,
exclusive parent fields, self-parenting and ancestor cycles inside the transaction.
A project/pillar cannot move below any descendant. Moving an ancestor moves its
whole subtree by changing its parent reference; preview affected descendants and
revalidate before commit. Moving an objective changes all its tasks' derived path.
Do not rewrite schedules, sprint selections, context assignments or work history.

Archive marks only the selected entity; descendants are *effectively archived*
while any owning ancestor is archived. Default active views exclude them, including
flat lists. Keep descendant lifecycle and explicit archive flags, allocations and
sprint selections intact. Show an impact summary including unfinished commitments;
never complete or reschedule them automatically. Restore removes the entity's
explicit archive flag; independently archived descendants stay archived. If an
ancestor remains archived, explain that the entity is still effectively archived
and offer restoring the ancestor or an explicit valid move. Do not restore others
silently. Moves into archived owners require an explicit warning/confirmation.
Deletion of organizational parents is deferred; reject removal with live children
until explicit reassignment. Archive/restore/move are undoable atomic commands.

## Worked Today, Done for Today, Completed

Use two quick check controls in active task rows: **Worked Today** and **Completed**.
Done for Today is a distinct row/menu action and day-closed badge, not the progress
checkbox. All views call the same shared commands and refresh from committed state.

| Operation | Transactional result / visible behavior |
| --- | --- |
| Worked Today on | Record a progress WorkEntry for this work target and today's calendar date, with zone and UTC recordedAt. Leave task lifecycle, completion timestamp and allocations unchanged. Repeated activation is idempotent for its quick-marker intent; additional session entries are allowed. |
| Worked Today off / Undo | Correct/void the quick-marker entry created by that command, retaining the original and correction history. If other work entries remain that date, keep the indicator checked and offer their history for explicit correction; do not erase all work silently. |
| Done for Today | For an active target, atomically record a work marker if none exists today, record a day-closure event/state, and close any existing daily allocations for today. Leave lifecycle active; create no allocation merely to record work. Show a separate closed-for-today state. |
| Resume Today | Reopen the day's disposition and the daily allocations closed by that closure command; retain progress history. Further work after closure prompts Resume Today rather than silently changing the day state. |
| Completed on | Complete a one-time/continuing task, or the selected recurring occurrence. Record completedAt and a completion history entry, close outstanding allocations in one transaction, and remove from default active results. It does not imply time spent or a progress entry today. |
| Completed off / Reopen | Explicit application command restores active lifecycle and clears the *current* completedAt projection, preserving the previous completion event. Do not silently reopen every historical allocation; select new work explicitly. Immediate Undo may restore exactly the allocations changed by completion when preconditions still match. |

Worked Today is derived from non-voided progress/session markers (including the
marker from Done for Today), not a persisted Task boolean. Closing a day is
independent of completing a task. Completed is derived from lifecycle/occurrence
status; completedAt must agree with the completed state. Undo/reopen are distinct:
Undo reverses the particular recent command; Reopen is a new lifecycle transition
available from history regardless of session undo availability.

Recommendation: use the configured calendar zone as the planning/local-day zone;
initialize it from the device zone and let users change it explicitly. Capture the
zone and workDate when the command starts. Crossing midnight while a dialog is open
requires a refreshed date/confirmation; never silently redate past work on travel,
clock change, undo or redo. Redo reapplies the original date intent. New days start
with their own progress/disposition; yesterday's history remains.

For recurring work the target is (taskId, occurrenceId). Work, day closure,
completion and undo apply to that instance, even when several missed instances are
visible today. The template is not completed by an instance checkbox; stopping the
series is a separate operation. Instance history stays attached to the nominal
occurrence identity after rescheduling. Template/occurrence validation arrives in
Phase 2; all commands reserve the target shape from the first persistence design.

## Contexts and ordering

Users define contexts; do not seed required Morning/Computer/etc. groups. Recommend
at most one live TaskContext assignment per task initially, always primary. Keep
the link-table shape for future secondary contexts, but do not expose multiple
assignment now. Context is optional and independent of organizational ownership.

Context management supports create/rename, archive/restore, delete and drag/button
reorder. Contexts have a persistent fractional sort key. Archive preserves links
and order; in default grouping, tasks whose primary context is archived are treated
as unassigned and still usable. Show the archived assignment in details; explicit
archive-inclusive views can show its group. Deleting a context tombstones it and
its assignments atomically after an impact confirmation; undo restores the same
IDs when safe. Tasks and ownership are never deleted. The final **No context**
group is synthetic, cannot be renamed/reordered/deleted, and always sorts last,
regardless of its tasks' numeric priorities. Empty groups may be hidden; drag mode
can reveal empty context targets. Restore resolves ordering collisions by stable ID.

**Recommended numeric priority:** default **5**, leaving 1–4 for more urgent work
without labeling every capture highest priority. Only positive integers; no
arbitrary product maximum. A proposed signed 64-bit representation has a documented
technical range (1..9223372036854775807), matching SQLite/MariaDB integer and
cross-platform serialization capabilities, with no silent clamping. Priority is a
Task property, independent of context order, due date, lifecycle and PeriodObjective
ranking. Recurring instances may override priority/order once Phase 2 exists.

Default context-grouped daily sort tuple:

1. Context assignment visibility: assigned groups first, No context last.
2. User-defined context sort key, then context UUID for equal keys.
3. Numeric task/occurrence priority ascending (1 highest).
4. Manual fractional ordering key **within that context/priority band**.
5. Stable work-target identity (task UUID; occurrence nominal date/key and UUID).

Context order wins even when Evening has priority 1 and Morning has priority 5.
The same base task order is used across dates/views, not an undocumented per-day
order override. An instance inherits the template's priority/key unless explicitly
overridden; several inherited instances are ordered deterministically by identity.

**Drag policy: combination of priority and separate manual position.** Choose the
immediate successor at the insertion line: adopt its numeric priority and insert
before it within that band. At the end use the predecessor's priority and insert
after it. At an empty destination retain the dragged task's priority. Remove the
moving row(s) before choosing anchors. Equal-priority drops change only manual
position; crossing priority bands **may change the dragged task's numeric priority**.
Never renumber unrelated priorities. Preview the exact context, resulting priority
and before/after neighbor; commit only on drop. Initially support single-row moves.

Use variable-length fractional rank tokens compared bytewise, not floating-point
numbers or client timestamps. Allocate a token between anchor keys, or before/after
an end. Freeze and test a canonical token-generation/comparison format before Phase
1.3 persistence; no library is added now. On collisions/token exhaustion, rebalance
only the affected tied/local band as needed, with all changed keys in the command
receipt. Global renumbering is a last resort, not routine. UUID ties give stable
convergence but are not a conflict-resolution substitute: server revision checks
must validate anchor/order assumptions. Cross-context drops update TaskContext,
priority and manual key together; undo/redo restores them as one logical operation.
Direct numeric-priority editing inserts at the end of the new priority band.

Context/priority assignment is task-level initially. A recurring-instance reorder
uses its priority/order override; changing its context affects the template and
all occurrences and must be presented as a series-wide context change, with an
explicit scope confirmation. Per-occurrence context overrides are deferred.

## Drag feedback and sorted/filtered views

Use Qt model/view flags, MIME data with stable IDs, supportedDropActions,
canDropMimeData/dropMimeData and move-row notifications. A proxy maps presentation
positions to source identities; row numbers are never domain IDs. Use the normal
view/delegate drop indicator or a small presentation override to draw an insertion
line **between** rows, not merely a highlighted destination. Show the destination
context in both the indicator label and status bar; support head/tail/empty groups
and a clear No context drop target. Dropping there removes the primary assignment.
No database mutation occurs during hover, autoscroll or drag initiation; Escape,
outside/invalid drops and cancelled drags leave every property unchanged.

Recommendation: enable reorder drops only in the default context/priority/manual
mode, with no row-hiding filters/search/owner scope or completed-history expansion.
A daily view is inherently date-scoped: anchors refer to its visible work targets,
while hidden targets retain keys; insert immediately before the visible successor
in the full band (or after the predecessor at the end). Preview names of those
anchors, not an absolute position in unseen dates. This is a deliberate ordering
operation, unlike merely applying a filter. Group collapse is allowed; header drops
mean end-of-group, explicitly previewed. Reject an ambiguous/changed anchor at commit.

For title/due-date sorts, narrowed filters or history views, explain why dropping
is disabled and offer “Switch to manual order / clear filters.” Selection/filtering
and header sorting alone never change saved priority/order. The task editor still
offers explicit context/priority commands in every valid view. Mobile may later
support equivalent intents without requiring desktop gestures.

## Date highlighting

Presentation preferences hold separate light/dark palette defaults, user overrides
and enabled rules. Proposed roles: overdue = dark red/light theme, light red/dark;
due today = dark amber/light, pale amber/dark; tomorrow = dark blue/light, light
blue/dark; later/no date = palette foreground; completed = muted palette foreground
with “Completed” text/check state. Validate contrast in the actual chosen palettes,
including selected rows and high-contrast modes; do not hardcode these as the only
colors users can choose. Due text/date badges also say Overdue/Today/Tomorrow;
completion may add strikeout. Color is never the sole signal.

Precedence: completed presentation suppresses urgency color; otherwise overdue,
today, tomorrow, later/no-date, in that mutually exclusive order. Work-progress and
Done for Today badges remain separate. Selection/focus contrast takes precedence
over custom colors while text/badges preserve meaning. Dates are dueDate (or an
occurrence override), not allocation dates. Future user highlighting rules can use
an ordered rule list after built-in lifecycle handling; no rule editor now.

## Filters and date boundaries

Let D be today's date in configured calendarTimeZone, tomorrow=D+1 calendar day,
and W the start of the week containing D according to weekStartsOn. Date-only
ranges are **[startDate, endDateExclusive)**; add calendar days, not fixed UTC
24-hour intervals. UI end dates are inclusive and convert to the following date
on save. Tasks without dueDate are excluded from every due-range filter.

| Filter | Due-date predicate |
| --- | --- |
| Overdue | dueDate < D |
| Due today | D <= dueDate < D+1 |
| Due by today | dueDate < D+1, including all overdue dates |
| Due tomorrow | D+1 <= dueDate < D+2 |
| Due by tomorrow | dueDate < D+2, including all overdue dates |
| Due by end of current week | dueDate < W+7, including overdue and earlier-week dates |
| Due during current sprint | selected current sprint start <= dueDate < endDateExclusive; excludes older overdue dates outside that range |
| Due during current quarter | current quarter start <= dueDate < endDateExclusive; same range rule |
| Without due dates | dueDate is null |

Default status scope is lifecycleStatus=active, excluding effective archive and
someday where appropriate. Phase 1.1 uses active/completed; later onHold/cancelled
require explicit additional status choices. “Show Completed” adds completed rows to
the current result; “Completed only” selects just them. It does not remove date,
context or ownership predicates. Explicit completed-only filtering replaces the
default active status predicate rather than ANDing an impossible combination.

Support primary context(s) including No context, numeric priority equality/range,
and pillar/project/objective filters. Recommendation: pillar and project selection
include descendants by default, with a direct-only toggle; objective selects its
tasks. Context filters consider the effective primary context as for grouping and
can explicitly include archived assignments. Retain a typed FilterSpec containing
status set, optional date criterion, context IDs, priority range, owner scope,
allocation period and sort/group modes. AND different criteria; OR values within
one selector. Keep due criteria single-choice initially. Future saved filters store
a versioned FilterSpec, not SQL text or a new query language.

“Scheduled this sprint” uses live allocations with status planned or doneForPeriod
to that sprint, or daily allocations whose dates fall within its interval,
deduplicated by task/occurrence. A broader quarter allocation alone is not a
sprint allocation. Do not
infer it from dueDate or objective selection. Broad sprint commitments do not
imply daily dates. Date changes recompute visible results; no task is moved merely
because a filter or period boundary changes. All date logic is shared/testable and
refreshes at zone midnight, preference changes and resume from suspension.

## Quarters and sprints

PlanningPeriod retains UUID, label, horizon, startDate and endDateExclusive.
Default quarters are Jan 1–Apr 1, Apr 1–Jul 1, Jul 1–Oct 1, Oct 1–Jan 1 of the
next year. Fiscal-quarter generation is deferred. Default sprints are calendar
months, e.g. November 2026 is [2026-11-01, 2026-12-01). Users may instead enter
custom inclusive start/end dates, converted to half-open storage bounds.

Recommendation: identify the current period by containing D; never silently generate
persisted periods as a side effect of filtering. With none, show “No current sprint/
quarter” and offer creation/selection. Permit overlapping custom periods, but when
more than one contains D require an explicit active-period selection and show its
label/bounds; do not choose arbitrarily. A remembered selection is valid as “current”
only while it contains D and is live. Outside that interval it is an explicitly
selected historical/future period, not current. Defaults for generated quarters
remain non-overlapping. Week/sprint/quarter boundaries need not form a strict tree.

Period-range edits preview affected filters and commitments, validate start<end,
and update the same UUID. Objective selections and direct period allocations remain
linked; dated allocations do not move, and out-of-range dated work is flagged for
review. Do not rewrite daily work history or automatically roll unfinished work.
Undo restores bounds when revision checks permit. A planning-period change and a
scheduling change are separate commands unless the user explicitly confirms a
single combined operation. Day bounds remain exactly one calendar day.
