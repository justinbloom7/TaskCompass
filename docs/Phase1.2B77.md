<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Phase 1.2 B7.7 — Persistent GUI acceptance

**Preparation only, 2026-11-18. No live B7.7 test, GUI write or manual acceptance
has been performed by the agent.** B7.5/final runtime privileges passed on both
platforms by owner report. The [milestone](milestones/Phase1.2.md) is the approval/
result authority. This package requests Linux QA first; Windows waits for review
and independent authorization. Preserve all original/rehearsal databases and data.

Use the [Linux checklist](Phase1.2B77Linux.md), then the
[Windows checklist](Phase1.2B77Windows.md). Checkboxes start unchecked; an offline
test or successful helper does not mark a manual GUI check passed.

## Exact operator sequence

From repository root, safe offline build/test (no live approval needed):

```bash
scripts/phase12/b77/01-build.sh
```

It delegates the existing Phase1.2 build with both live-test options OFF. Linux
uses build; Windows UCRT64 uses build-ucrt64. For review of explicit commands:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
   -DTASKCOMPASS_ENABLE_DB_TESTS=OFF -DTASKCOMPASS_ENABLE_PHASE12_DB_TESTS=OFF
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

Windows substitutes build-ucrt64 and uses the installed UCRT64 Qt/CMake prefix,
as handled by01-build.sh. Never run reset-capable live integration tests against
any development or preserved rehearsal database.

**After Linux B7.7 approval**, close all TaskCompass GUI/client sessions:

```bash
scripts/phase12/b77/02-preflight.sh --approved --platform linux
```

It verifies the complete sealed B7.5 evidence and exact current schema2/UUID/data/
final grants/DDL/health; a changed baseline stops before testing. It creates a new
private b77-linux-timestamp-pid directory beside the preserved B7.5 snapshot and
prints its path and QA prefix. It creates local test-name templates, not database
records. Assign the actual printed path once (genuinely required information):

```bash
qaDirectory='/paste/the/printed/private/b77-directory'
```

Copy titles from `$qaDirectory/qa-names.tsv`; **every** new organizational name/
task title must retain the printed prefix plus a space, including renamed records.
Open `$qaDirectory/protected-inventory.tsv` privately to identify original tasks,
completed states and tombstones. Do not post personal task data or credentials.

Launch the persistent application (TaskCompass, not TaskCompassPrototype):

```bash
scripts/phase12/b77/03-launch.sh --approved --platform linux --qa-dir "$qaDirectory"
```

Follow checklist G01–G07, wait for all operations to finish, then close normally:

```bash
scripts/phase12/b77/04-checkpoint.sh --approved --platform linux \
   --qa-dir "$qaDirectory" --label before-restart
```

The checkpoint prints the private inventory.tsv containing UUIDs, owners, explicit/
effective archive flags, tombstones, status and revisions. Record each QA role's
UUID there or in private observations; names alone are not identities.

Restart using the **same** launch command, perform G08 without mutations, close:

```bash
scripts/phase12/b77/03-launch.sh --approved --platform linux --qa-dir "$qaDirectory"
scripts/phase12/b77/04-checkpoint.sh --approved --platform linux \
   --qa-dir "$qaDirectory" --label after-restart --compare before-restart
```

Run the two-session G09 conflict tests only on QA records. Launch03 separately in
two terminals with the same qaDirectory; close **both** sessions afterward:

```bash
scripts/phase12/b77/04-checkpoint.sh --approved --platform linux \
   --qa-dir "$qaDirectory" --label final
```

Report the manual checklist plus helper outputs and inventory paths. Windows uses
identical commands with --platform windows only after Linux review and separate
approval; create its own run directory/prefix, never reuse Linux paths/passwords.
No phrase/target/yes-no confirmation is requested by scripts; passwords only.
Existing product move/archive warning dialogs are intentional impact previews,
not added script approval prompts. No grants, migrations, SQL DML or cleanup are
performed by these helpers; deliberate approved GUI commands are the only writers.

## QA record plan and original-data protection

Before any GUI mutation, select All tasks, clear search, inspect original active
and completed work against the private protected inventory; tombstones must remain
absent from GUI even with Show Completed. Read-only captures verify their exact
original fields and historical records. Never complete/reopen/rename/move/archive
an original task or add QA children under an original organizational record.

Suggested QA tree (prefix every displayed title using qa-names.tsv):

```text
Root A (pillar)
├─ Sub 1 (subpillar)
│  ├─ Sub 2 (subpillar)
│  │  └─ Project
│  │     ├─ Subproject
│  │     │  └─ Project outcome (objective) → Objective task
│  │     └─ Direct task
│  └─ Subpillar outcome (objective)
└─ Pillar outcome (objective)
Root B (pillar)
Independent task (no objective/project)
```

Add two identical QA Duplicate objective names under different QA owners, a History
root for undo-create tests, and disposable QA-only missing-parent/conflict records.
No objective contains another objective; no task belongs directly to a pillar.
Move Project to Root B as the move test; children/tasks retain their UUID/owner
links while their derived path changes. Maintain the prefix on every rename.
After G01, put the QA prefix in Ctrl+F before editing/completing tasks so original
tasks are filtered out. Notes may differ freely on QA records. Do not delete QA rows; Undo Create is a
recorded soft removal of newly created QA data, not a purge or cleanup.

There is no special QA-only GUI mode: the operator must select only QA records.
Helpers detect any original-data change after close and stop; they never undo or
restore it automatically.

The database baseline is immutable B7.5 `b75-final.hex.tsv`, not a new baseline
that could hide accidental edits. Every original row in **all nine tables** must
remain byte-for-byte unchanged, including completed/tombstoned tasks, raw audit
JSON/outbox payloads and schema/journal metadata. Counts may grow only through
new prefix-qualified QA domain records and their new command/outbox records.

The offline validator additionally rejects new non-QA records, cross-dataset or
contradictory/missing owners, hierarchy cycles, live children under removed parents,
QA attached to protected owners, writes/audits targeting original records, mismatched
command/outbox operations/payloads/revisions and inverses referencing original
history. No count-only or title-only preservation claim. Health SQL separately
checks every CHECK/FK predicate; Qt independently validates registered structure,
fingerprint/journal/dataset and final runtime privileges are inspected natively.

## Source-reviewed coverage, limits and potential gaps

| Area | Implemented/source evidence | Acceptance treatment |
| --- | --- | --- |
| Hierarchy/owners | Organize menu, context actions, parent picker, model/graph and transaction validators | G02/G03; no production feature missing found in this review; manual not yet verified |
| Rename/edit | Edit / Rename dialog; parent change uses Move | Do not expect inline tree F2 editing; no UI redesign in this preparation |
| Archive/restore | Explicit flags, inherited filtering, Locate Archived Ancestor and independent task lifecycle | G05; restoring child under archived parent does not restore ancestors |
| Identity | Navigation tooltip/type+UUID disambiguation; tasks carry UUID internally | Task UUID is not a visible dialog field; helper inventory supplies it, no Copy ID control added |
| Tombstones | Hidden from all normal task lists | Verify exact preservation through exports, not an invented Show Deleted control |
| History | One typed session stack, max100 entries, durable command/outbox history | Keep manual sequence below100; undo values restore while revisions/timestamps advance |
| Conflict handling | Stale revision/workspace/dependency guards; failed command/inverse does not advance cursor | Failed conflict is not an automatic merge; G09 must preserve sessionB's change |
| History invalidation | Successful mutation observing an externally changed tracked revision clears older history; new commit invalidates redo; unknown commit clears history | Reload alone does **not** clear the stack. A conflicting inverse can remain available and reject again. This differs from blanket "invalidate on any conflict"; owner should review this precise policy, not silently change it |
| Connection recovery | Worker disables editing on error; F5 reads via the retained session | Possible recovery limitation: F5 does not recreate a dead session/connection. Native interruption recovery is unverified; close/relaunch is the safe fallback. Propose a targeted reconnect/session-reset fix only if confirmed, preserving unknown-commit rules |
| Display preferences | Selection/expansion/view/geometry/splitter persisted; archive filter is session-only | G08. Show Completed resets off by design; per-user settings are shared between simultaneous windows, so measure restart after only one main session |
| Permissions/interruption | Offline failure/unknown-commit and atomicity tests; prior live isolated rollback/lease tests | No intentional server stop, session KILL or grant revoke on valuable dev data. True live GUI interruption/permission-denial remains unexecuted; separate safe fault-test scope needed if owner requires it |
| Deferred features | No user deletion, project/objective completion, contexts, numeric ordering, progress, dates/recurrence/planning/sync | Not Phase1.2 defects; do not test or implement Phase1.3+ controls |

The initial acceptance package changed no production behavior. The owner-authorized
2026-11-18 usability refinement now updates creation defaults/menus, type labels,
grouped alphabetical navigation and command-backed organizational drops; see
[focused retest](TestingPhase1.2.md#focused-usability-retest--2026-11-18). If a
manual result exposes a defect, stop that scenario, record it and propose the
smallest targeted source fix for review rather than changing accepted semantics.

## Manual acceptance procedure (all steps need owner observation)

### G01 — Legacy views and interaction baseline

- [ ] All tasks + cleared search shows original active records; Show Completed
  reveals original completed records with unchanged completion dates; no tombstones
  appear. Inspect only; never toggle an original checkbox.
- [ ] Sorting/column sizing/row selection/keyboard/double-click edit/Ctrl+F work;
  cancel an original task dialog without saving. Show Navigation and splitter work.
- [ ] No unexpected pending error/busy state. Platform/Qt/server/source revision is captured automatically by the scripts
  in [build notes](BuildNotes.md); no manual version collection is required.

### G02 — Create/edit/rename and navigation

- [ ] Compare + and right-click creation menus with the focused retest checklist.
  New Task is first; only legal child actions appear; New Pillar is last in the
  creation section. Check separator, all five icon/type tooltip/details and grouped
  case-insensitive sibling sorting.
- [ ] Create the QA tree above through Organize → New Pillar/Project/Objective and
  Choose Parent. Pillars may be roots or subpillars; project/objective require a
  legal parent. Use Edit / Rename to change QA title/description and retain prefix.
- [ ] Navigate at least five mixed levels; expand/collapse, select and change to
  Objective-oriented navigation. Objectives are the same UUIDs with owner context,
  not duplicated records. Duplicate objective names are disambiguated by type/UUID.
- [ ] Under archived selection New Pillar may default to a root; read its Parent
  field. Do not mistake this explicit root choice for a child under archive.

### G03 — Task owners and invalid choices

- [ ] New Task defaults to the selected active objective/project, including the
  Task menu and + menu (2026-11-18 refinement supersedes the original global-
  independent default). Global/pillar/archived selection defaults Independent.
  Choose Owner always retains Independent; verify all entry points.
- [ ] Edit QA task owner Independent → Direct Project → Objective → Independent;
  save each, verify UUID stable and exactly one owner or neither. Pillars are
  containers only in task picker, never selectable task owners.
- [ ] Reject rootless project/objective, objective parent for any organization,
  pillar under project, self/descendant moves and creation under archived owners.
  Unavailable choices/disabled OK count as UI rejection; do not bypass with SQL.
- [ ] Empty/whitespace title disables OK. A >1024 UTF-8-byte QA title or >65535-byte
  notes must reject transactionally (if tested, generate text locally, not SQL).
  Cancel creates no row. On application validation error, use F5 to re-enable edits.

### G04 — Moves and shared undo/redo

- [ ] Retest organization drops onto valid pillar/project parents: cancel preview
  first, then complete/Undo/Redo. Invalid/cyclic/stale/between-row drops must not
  write. Archived destinations warn. Root promotion uses Move; objective-view
  dragging and all task/priority/manual-order dragging are disabled.

- [ ] Move QA Project subtree from Sub2 to RootB; inspect impact counts (including
  hidden/completed work), then Undo/Redo. Move QA objective between pillar/project
  and move a QA subpillar subtree between pillars. No self/descendant destination.
- [ ] Record target/child/task UUIDs: only moved root's parent/revision changes;
  descendant explicit archive flags, IDs/owners and task completion stay unchanged.
- [ ] Test Undo/Redo for create, title/description edit, move, archive, restore,
  task ownership, QA completion/reopening. A mixed sequence (organization rename,
  task edit, archive) undoes strictly last-in-first-out across both entity types.
  New successful QA edit after Undo disables Redo. No partial UI change before commit.

### G05 — Explicit versus inherited archive

- [ ] Explicitly archive QA Subproject, then RootB. Default views hide inherited
  descendants/tasks; Show Archived exposes [archived]/[inherited archive] labels.
- [ ] Restore Subproject while RootB stays archived: explicit child flag clears
  but child/task remain effectively archived. Locate Archived Ancestor selects B.
- [ ] Archive Subproject again; restore RootB: child explicit archive remains.
  Restore child separately; no descendant flag silently rewritten.
- [ ] Move QA work into archived owner only via warned Move workflow; moving it
  out removes inherited archive where no other explicit flag applies. Creation
  into archive remains blocked. Undo/Redo archive/restore/move preserve distinctions.
- [ ] Leave Subproject explicitly archived for restart testing; keep RootB active.

### G06 — Completion and context visibility

- [ ] Complete QA Direct task; completedAt appears, default active list hides it,
  Show Completed reveals it. Reopen clears current completedAt. Undo/Redo retain
  durable history; finish with this QA task completed for restart observation.
- [ ] Test Show Archived and Show Completed independently and together. Archived
  context does not complete tasks; completed task lifecycle does not archive owners.

### G07 — Commit/checkpoint

- [ ] Wait until Working... clears; record selected UUID and expanded path. Select
  a visible archived QA entity with Show Archived on for restart. Close normally.
- [ ] Run before-restart checkpoint; capture private UUID inventory and ownership/
  revision/archive states. PASS covers machine invariants, not your visual checklist.

### G08 — Restart (read-only interval)

- [ ] Relaunch with same runtime helper. All QA hierarchy/task relationships,
  explicit/inherited archive and completed states persist; originals unchanged.
- [ ] Undo and Redo start disabled; durable history remains. Show Completed starts
  off; Show Archived also starts off regardless of its prior value. View, visible
  selection, expanded paths, geometry and splitter restore where applicable; a
  hidden archived selection falls back to All Tasks.
- [ ] Perform only navigation/filter/Reload observations, no mutations; close.
  Run after-restart --compare before-restart: **all nine tables must be identical**.

### G09 — Two-session QA conflicts and missing parents

- [ ] In A, edit a QA task but leave dialog unsaved. B saves an edit to the same
  QA task. A saves: stale operation rejects, B's data stays, editing disables until
  explicit F5. No failed command receipt/outbox row. Do not touch original tasks.
- [ ] A makes a QA edit, B changes that tracked QA record, A Undo rejects instead
  of overwriting B. Cursor stays unchanged. After F5, a fresh successful A mutation
  on the changed record invalidates old tracked history as documented. An ordinary
  new edit after Undo invalidates redo; restart clears all session history.
- [ ] Missing-parent test: B creates a new empty QA project; A reloads and opens
  unsaved New Task Under Selection; B Undo Create removes that QA project softly.
  A saves: stale/missing parent rejects atomically. Keep the project tombstone/history.
- [ ] Undo-create dependency test: A creates empty QA History root; B reloads and
  adds a QA child (or completed QA task under its QA project). A Undo must refuse
  removing a nonempty parent. Failure must not partially persist or advance cursor.
- [ ] Close both sessions after queues settle; final checkpoint must preserve all
  protected rows and pair every new command with one consistent new outbox record.

## Error boundaries, defects and retest

Covered offline: invalid/cross-dataset parents, cycles/deep graphs, stale edits and
inverses, failed transactions/rollback, lost commit acknowledgment/history clearing,
identity guards and disabled editing on failure. Prior isolated live suites covered
real separate-connection lease timeout/loss/conflicting moves; **do not rerun them**
against preserved data. Database connection interruption and permission-denied GUI
mutations are **not claimed manually passed**. No intentional live fault injection,
service control, SQL KILL, authentication/grant changes or reset is in this approval.
If owner requires those manual cases, propose an independent safe test scope first.

On any unexpected result stop the affected test, preserve actual state, both GUI
sessions' observations and every artifact. Unknown commit: no blind retry; close
GUI after work settles, run a read-only checkpoint if safe and review actual QA
records/audit. A failed checkpoint leaves an unsealed directory; helpers then
refuse further launch/checkpoint until its specific state is reviewed. Never delete
it or rebaseline to hide a mismatch. No automatic repair/cleanup.

Defect report: test ID, platform/build/Qt/server, timestamp, QA prefix/UUIDs, action
and expected/actual result, safe error text, last successful checkpoint, whether
commit is uncertain, reproduction and screenshot without personal data/secrets.
Propose the smallest fix and test; obtain review for semantics changes. Retest with
fresh checkpoint labels and this preserved QA prefix/IDs, adding uniquely suffixed
QA titles if needed. Do not repeat initial02 after GUI writes: it deliberately
requires pristine B7.5. Resume03/04 on the same run directory instead. Preserve QA
records, including undo-create tombstones, until separately approved cleanup.

## Completion and remaining gates

B7.7 requires owner-observed Linux checklist/results plus machine checks; review
Linux before authorizing Windows. Windows needs its own checklist/UUID inventory/
restart equality. Explicitly record manual fault cases not executed and resolve
blocking defects/policy questions rather than silently marking them passed.
Then prepare separately approved B7.8 populated schema2 backup, B7.9 independent
restore/compare (preserving rehearsal/old integration databases), and B7.10 owner
review/final milestone acceptance. Phase1.3 is not authorized.
